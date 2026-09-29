// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Abilities/VeyraRideAbility.h"

#include "AbilitySystemComponent.h"
#include "Delivery/VeyraEffectDelivery.h"
#include "Delivery/VeyraProjectile.h"
#include "Engine/World.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Movement/VeyraMovementComponent.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraAbilitiesLog.h"
#include "VeyraAbilitiesVerbs.h"
#include "VeyraCombatVerbs.h"

bool UVeyraRideAbility::Defines(const FVeyraContentId& Ability) const
{
	return UVeyraAbilitiesTuningSubsystem::FindRide(Ability) != nullptr;
}

double UVeyraRideAbility::GetResourceCost(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraRideAbilityTuning* Ride = UVeyraAbilitiesTuningSubsystem::FindRide(Ability);
	return Ride ? VeyraAbilityRules::ValueAtRank(Ride->Cast.ResourceCostByRank, Rank) : 0.0;
}

double UVeyraRideAbility::GetCooldownSeconds(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraRideAbilityTuning* Ride = UVeyraAbilitiesTuningSubsystem::FindRide(Ability);
	return Ride ? VeyraAbilityRules::ValueAtRank(Ride->Cast.CooldownSecondsByRank, Rank) : 0.0;
}

EVeyraCastRejection UVeyraRideAbility::CheckTarget(const AActor& /*Caster*/, const FVeyraContentId& Ability, const FVeyraCastTarget& /*Target*/) const
{
	return UVeyraAbilitiesTuningSubsystem::FindRide(Ability) ? EVeyraCastRejection::None : EVeyraCastRejection::UnknownAbility;
}

const FVeyraCastTuning* UVeyraRideAbility::GetCastTuning(const FVeyraContentId& Ability) const
{
	const FVeyraRideAbilityTuning* Ride = UVeyraAbilitiesTuningSubsystem::FindRide(Ability);
	return Ride ? &Ride->Cast : nullptr;
}

bool UVeyraRideAbility::IsOffensive(const FVeyraContentId& /*Ability*/) const
{
	// Mounting is no attack; its mounted actions answer for themselves.
	return false;
}

FVeyraChannelPlan UVeyraRideAbility::Deliver(const FVeyraCast& Cast)
{
	const FVeyraRideAbilityTuning* Tuning = UVeyraAbilitiesTuningSubsystem::FindRide(Cast.Ability);
	UAbilitySystemComponent* Caster = Cast.Caster.Get();
	const AActor* Body = Caster ? Caster->GetAvatarActor() : nullptr;
	UVeyraMovementComponent* Movement = Body ? Body->FindComponentByClass<UVeyraMovementComponent>() : nullptr;
	UWorld* World = GetWorld();
	if (!Tuning || !Caster || !Movement || !World)
	{
		return FVeyraChannelPlan();
	}
	// A newer ride replaces the older: the older's statuses and mounted actions go first.
	if (Rider.IsValid())
	{
		VeyraCombat::EndRide(*Caster, EVeyraRideEndReason::Dismounted);
	}
	if (!VeyraCombat::StartRide(*Caster, FVeyraRide{ Tuning->SetSpeed, Tuning->TurnRateDegreesPerSecond, Tuning->DecaySeconds }))
	{
		UE_LOG(LogVeyraAbilities, Warning, TEXT("%s could not ride for %s (cast %d)."), *GetNameSafe(Body), *Cast.Ability.ToString(), Cast.CastId);
		return FVeyraChannelPlan();
	}
	Rider = Caster;
	RideAbility = Cast.Ability;
	RideRank = Cast.Rank;
	RideCastId = Cast.CastId;
	const int32 Level = GetCasterLevel(*Caster);
	for (const FVeyraContentId& StatusId : Tuning->RiderStatuses)
	{
		if (TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(StatusId, Level))
		{
			Status->DurationSeconds = Tuning->DurationSeconds;
			VeyraCombat::ApplyStatus(*Caster, *Caster, Status.GetValue());
		}
	}
	// Its mounted actions hold their slots, cooling down apart from the actions they replace (§56).
	if (UVeyraAbilityLoadoutComponent* Loadout = Caster->GetOwner() ? Caster->GetOwner()->FindComponentByClass<UVeyraAbilityLoadoutComponent>() : nullptr)
	{
		for (const FVeyraRideSlotTuning& Mounted : Tuning->Mounted)
		{
			FVeyraOverrideSpec Spec;
			Spec.Ability = Mounted.Ability;
			Spec.DurationSeconds = Tuning->DurationSeconds;
			Spec.Use = EVeyraOverrideUse::WhileActive;
			Spec.Group = MountedGroup();
			Loadout->Override(*Caster, Mounted.Slot, Spec);
		}
	}
	Watched = Movement;
	RideEndedHandle = Movement->OnRideEnded.AddUObject(this, &UVeyraRideAbility::OnRideEnded);
	if (UVeyraCombatEventSubsystem* Events = World->GetSubsystem<UVeyraCombatEventSubsystem>())
	{
		DeathHandle = Events->OnDeath.AddUObject(this, &UVeyraRideAbility::OnDeath);
	}
	// Bound to the rider, not to this ability: GAS clears an ability's own timers as its cast ends.
	TWeakObjectPtr<UVeyraRideAbility> Self(this);
	World->GetTimerManager().SetTimer(ExpiryTimer, FTimerDelegate::CreateWeakLambda(Caster, [Self]() {
		if (UVeyraRideAbility* Ability = Self.Get())
		{
			Ability->Expire();
		}
	}), static_cast<float>(Tuning->DurationSeconds), /*bLoop*/ false);
	return FVeyraChannelPlan();
}

void UVeyraRideAbility::Expire()
{
	UAbilitySystemComponent* Caster = Rider.Get();
	const FVeyraCastTuning* CastTuning = GetCastTuning(RideAbility);
	if (!Caster)
	{
		StopWatching();
		return;
	}
	// A recast that fires at expiry fires as the time runs out, whichever timer comes first (Roster
	// Bible §1: Last Exit's payoff is guaranteed). It leaves the ride itself.
	const UVeyraAbilityLoadoutComponent* Loadout = Caster->GetOwner() ? Caster->GetOwner()->FindComponentByClass<UVeyraAbilityLoadoutComponent>() : nullptr;
	const bool bFiresAtExpiry = CastTuning && !CastTuning->RecastWindow.IsEmpty() && CastTuning->RecastWindow[0].OnExpiry == EVeyraRecastExpiry::Cast;
	const FVeyraLoadoutEntry* FollowUp = bFiresAtExpiry && Loadout ? Loadout->FindAbility(CastTuning->RecastWindow[0].Ability) : nullptr;
	const FVeyraLoadoutEntry* Holding = FollowUp ? Loadout->FindSlot(FollowUp->Slot) : nullptr;
	if (Holding && Holding->Ability == CastTuning->RecastWindow[0].Ability)
	{
		const AActor* Body = Caster->GetAvatarActor();
		const UVeyraMovementComponent* Movement = Watched.Get();
		FVeyraCastTarget Ahead;
		Ahead.bHasLocation = Body != nullptr;
		Ahead.Location = Body ? Body->GetActorLocation() + (Movement ? Movement->GetRideHeading() : Body->GetActorForwardVector()) * Body->GetSimpleCollisionRadius() * 4.0
			: FVector::ZeroVector;
		VeyraAbilities::TryCast(*Caster, Holding->Slot, Ahead);
	}
	if (Rider.IsValid() && VeyraCombat::IsRiding(*Caster))
	{
		VeyraCombat::EndRide(*Caster, EVeyraRideEndReason::Expired);
	}
}

void UVeyraRideAbility::OnRideEnded(const FVeyraRideEnd& End)
{
	UAbilitySystemComponent* Caster = Rider.Get();
	const FVeyraContentId Ability = RideAbility;
	const FName Group = MountedGroup();
	StopWatching();
	const FVeyraRideAbilityTuning* Tuning = UVeyraAbilitiesTuningSubsystem::FindRide(Ability);
	if (!Caster || !Tuning)
	{
		return;
	}
	for (const FVeyraContentId& Status : Tuning->RiderStatuses)
	{
		VeyraCombat::RemoveStatus(*Caster, Status);
	}
	if (UVeyraAbilityLoadoutComponent* Loadout = Caster->GetOwner() ? Caster->GetOwner()->FindComponentByClass<UVeyraAbilityLoadoutComponent>() : nullptr)
	{
		Loadout->EndGroup(*Caster, Group);
	}
	// On every exit its vehicle goes on without its rider (§56).
	if (!Tuning->Vehicle.IsEmpty())
	{
		LaunchVehicle(*Caster, Tuning->Vehicle[0], End);
	}
	// A recast that separates rider and vehicle is under way; any other end leaves it nothing to do.
	if (End.Reason != EVeyraRideEndReason::Dismounted)
	{
		EndRecastWindow(*Caster, Ability);
	}
}

void UVeyraRideAbility::OnDeath(const FVeyraDeathEvent& Death)
{
	if (UAbilitySystemComponent* Caster = Rider.Get(); Caster && Death.Victim.Get() == Caster)
	{
		VeyraCombat::EndRide(*Caster, EVeyraRideEndReason::Died);
	}
}

void UVeyraRideAbility::LaunchVehicle(UAbilitySystemComponent& Caster, const FVeyraContentId& Vehicle, const FVeyraRideEnd& End) const
{
	const FVeyraSkillshotAbilityTuning* Skillshot = UVeyraAbilitiesTuningSubsystem::FindSkillshot(Vehicle);
	UWorld* World = GetWorld();
	if (!Skillshot || !World)
	{
		return;
	}
	// A projectile: it passes through units, once per enemy, and terrain stops it; its hits are its rider's,
	// even a dead rider's (§56).
	const FTransform Launch(End.Heading.Rotation(), End.Location);
	if (AVeyraProjectile* Projectile = World->SpawnActor<AVeyraProjectile>(AVeyraProjectile::StaticClass(), Launch))
	{
		Projectile->LaunchLine(Caster, End.Heading, Skillshot->Projectile, Skillshot->Collision, VeyraEffectDelivery::Prepare(Caster, Skillshot->Effects, RideRank),
			VeyraEffectDelivery::Prepare(Caster, Skillshot->PassThroughEffects, RideRank), Vehicle, RideCastId);
	}
}

FName UVeyraRideAbility::MountedGroup() const
{
	return FName(*FString::Printf(TEXT("ride_%s"), *RideAbility.ToString()));
}

void UVeyraRideAbility::StopWatching()
{
	if (UVeyraMovementComponent* Movement = Watched.Get())
	{
		Movement->OnRideEnded.Remove(RideEndedHandle);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ExpiryTimer);
		if (UVeyraCombatEventSubsystem* Events = World->GetSubsystem<UVeyraCombatEventSubsystem>())
		{
			Events->OnDeath.Remove(DeathHandle);
		}
	}
	Watched.Reset();
	RideEndedHandle.Reset();
	DeathHandle.Reset();
	Rider.Reset();
}
