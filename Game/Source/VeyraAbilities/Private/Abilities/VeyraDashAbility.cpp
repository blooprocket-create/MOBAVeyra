// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Abilities/VeyraDashAbility.h"

#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "Delivery/VeyraAreaDelivery.h"
#include "Engine/World.h"
#include "Movement/VeyraMovementComponent.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraAbilitiesLog.h"
#include "VeyraCombatVerbs.h"

bool UVeyraDashAbility::Defines(const FVeyraContentId& Ability) const
{
	return UVeyraAbilitiesTuningSubsystem::FindDash(Ability) != nullptr;
}

double UVeyraDashAbility::GetResourceCost(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraDashAbilityTuning* Dash = UVeyraAbilitiesTuningSubsystem::FindDash(Ability);
	return Dash ? VeyraAbilityRules::ValueAtRank(Dash->Cast.ResourceCostByRank, Rank) : 0.0;
}

double UVeyraDashAbility::GetCooldownSeconds(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraDashAbilityTuning* Dash = UVeyraAbilitiesTuningSubsystem::FindDash(Ability);
	return Dash ? VeyraAbilityRules::ValueAtRank(Dash->Cast.CooldownSecondsByRank, Rank) : 0.0;
}

EVeyraCastRejection UVeyraDashAbility::CheckTarget(const AActor& Caster, const FVeyraContentId& Ability, const FVeyraCastTarget& Target) const
{
	const FVeyraDashAbilityTuning* Dash = UVeyraAbilitiesTuningSubsystem::FindDash(Ability);
	if (!Dash)
	{
		return EVeyraCastRejection::UnknownAbility;
	}
	if (Dash->Direction == EVeyraDashDirection::ThroughTarget)
	{
		// It names the enemy unit it passes through, within its reach (ADR-030 §6).
		return CheckEnemyUnit(Caster, Target.Actor.Get(), Dash->Cast.CastRange, Dash->TargetKinds);
	}
	if (Dash->Direction == EVeyraDashDirection::AwayFromHost)
	{
		// It throws its caster off the unit it holds on to, which gives the dash its direction.
		const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Caster);
		return AbilitySystem && VeyraCombat::GetAttachHost(*AbilitySystem) ? EVeyraCastRejection::None : EVeyraCastRejection::InvalidTarget;
	}
	// The point gives the dash its direction.
	return HasUsablePoint(Target) ? EVeyraCastRejection::None : EVeyraCastRejection::InvalidLocation;
}

bool UVeyraDashAbility::TakesOverDash(const FVeyraContentId& Ability) const
{
	const FVeyraDashAbilityTuning* Dash = UVeyraAbilitiesTuningSubsystem::FindDash(Ability);
	return Dash && Dash->DuringDash == EVeyraDuringDash::TakesOver;
}

const FVeyraCastTuning* UVeyraDashAbility::GetCastTuning(const FVeyraContentId& Ability) const
{
	const FVeyraDashAbilityTuning* Dash = UVeyraAbilitiesTuningSubsystem::FindDash(Ability);
	return Dash ? &Dash->Cast : nullptr;
}

FVeyraChannelPlan UVeyraDashAbility::Deliver(const FVeyraCast& Cast)
{
	const FVeyraDashAbilityTuning* Dash = UVeyraAbilitiesTuningSubsystem::FindDash(Cast.Ability);
	UAbilitySystemComponent* Caster = Cast.Caster.Get();
	const AActor* Body = Caster ? Caster->GetAvatarActor() : nullptr;
	UWorld* World = GetWorld();
	if (!Dash || !Caster || !Body || !World)
	{
		return FVeyraChannelPlan();
	}

	// The start zones hit where the caster sets off, facing the point.
	if (!Dash->StartZones.IsEmpty())
	{
		FVeyraEffectFrame Placement;
		Placement.Origin = Body->GetActorLocation();
		Placement.Direction = Cast.Direction;
		Placement.bOriginIsCaster = true;
		VeyraAreaDelivery::Resolve(*World, *Caster, Placement, VeyraAreaDelivery::PrepareZones(*Caster, Dash->StartZones, Cast.Rank),
			FVeyraAbilityHitSource{ Cast.Ability, Cast.CastId });
	}

	FVector Heading = VeyraAbilityRules::DashHeading(*Dash, Cast.Direction);
	double Distance = Dash->Distance;
	if (Dash->Direction == EVeyraDashDirection::ThroughTarget)
	{
		// Through the named unit to Distance beyond it; it takes the contact effects as the dash sets off.
		AActor* Through = Cast.TargetActor.Get();
		if (!Through || !VeyraTargeting::IsAlive(Through))
		{
			return FVeyraChannelPlan();
		}
		const FVector Between = (Through->GetActorLocation() - Body->GetActorLocation()) * FVector(1.0, 1.0, 0.0);
		Heading = Between.IsNearlyZero() ? Body->GetActorForwardVector().GetSafeNormal2D() : Between.GetSafeNormal();
		Distance = Between.Size() + Dash->Distance;
		FVeyraEffectFrame Frame;
		Frame.Origin = Body->GetActorLocation();
		Frame.Direction = Heading;
		Frame.bOriginIsCaster = true;
		VeyraEffectDelivery::Apply(*Caster, *Through, VeyraEffectDelivery::Prepare(*Caster, Dash->ContactEffects, Cast.Rank), Frame,
			FVeyraAbilityHitSource{ Cast.Ability, Cast.CastId });
	}
	if (Dash->Direction == EVeyraDashDirection::AwayFromHost)
	{
		// Straight back from its host, letting go; the host takes the host effects, pushed from the caster.
		AActor* Host = VeyraCombat::GetAttachHost(*Caster);
		if (!Host)
		{
			return FVeyraChannelPlan();
		}
		Heading = (Body->GetActorLocation() - Host->GetActorLocation()).GetSafeNormal2D();
		if (Heading.IsNearlyZero())
		{
			Heading = -Host->GetActorForwardVector().GetSafeNormal2D();
		}
		const FVeyraPreparedEffects HostEffects = VeyraEffectDelivery::Prepare(*Caster, Dash->HostEffects, Cast.Rank);
		VeyraCombat::Detach(*Caster);
		FVeyraEffectFrame Frame;
		Frame.Origin = Body->GetActorLocation();
		Frame.Direction = -Heading;
		Frame.bOriginIsCaster = true;
		VeyraEffectDelivery::Apply(*Caster, *Host, HostEffects, Frame, FVeyraAbilityHitSource{ Cast.Ability, Cast.CastId });
	}
	UVeyraMovementComponent* Movement = Body->FindComponentByClass<UVeyraMovementComponent>();
	const bool bLeavesRide = Dash->RideExit == EVeyraRideExit::Leave && VeyraCombat::IsRiding(*Caster);
	TSharedPtr<FPendingContact> Pending;
	if ((Dash->Contact == EVeyraDashContact::StopAtFirstEnemy || !Dash->EndZones.IsEmpty() || bLeavesRide) && Movement)
	{
		Pending = MakeShared<FPendingContact>();
		Pending->bLeavesRide = bLeavesRide;
		Pending->Caster = Caster;
		Pending->Source = FVeyraAbilityHitSource{ Cast.Ability, Cast.CastId };
		Pending->Direction = Heading;
		Pending->Effects = VeyraEffectDelivery::Prepare(*Caster, Dash->ContactEffects, Cast.Rank);
		for (const FVeyraContentId& StatusId : Dash->ContactSelfStatuses)
		{
			if (const TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(StatusId))
			{
				Pending->SelfStatuses.Add(Status.GetValue());
			}
		}
		Pending->EndZones = VeyraAreaDelivery::PrepareZones(*Caster, Dash->EndZones, Cast.Rank);
		Pending->Movement = Movement;
		// Bound to the caster: it runs once, when this dash ends, and lets go of itself.
		Pending->Handle = Movement->OnDashEnded.AddWeakLambda(Caster, [Pending](const FVeyraDashEnd& End) {
			// Held first: removing this binding destroys the lambda, and what it captured with it.
			const TSharedPtr<FPendingContact> Held = Pending;
			if (UVeyraMovementComponent* Moved = Held->Movement.Get())
			{
				Moved->OnDashEnded.Remove(Held->Handle);
			}
			Land(*Held, End);
		});
	}
	if (!VeyraCombat::Dash(*Caster, FVeyraDash{ Heading, Distance, Dash->Speed, Dash->Contact, Dash->DuringDash == EVeyraDuringDash::TakesOver }))
	{
		UE_LOG(LogVeyraAbilities, Verbose, TEXT("%s could not dash for %s (cast %d)."), *GetNameSafe(Body), *Cast.Ability.ToString(), Cast.CastId);
		if (Pending.IsValid())
		{
			Movement->OnDashEnded.Remove(Pending->Handle);
		}
		// It still leaves the ride, where it stands.
		if (bLeavesRide)
		{
			VeyraCombat::EndRide(*Caster, EVeyraRideEndReason::Dismounted);
		}
	}
	return FVeyraChannelPlan();
}

void UVeyraDashAbility::Land(const FPendingContact& Pending, const FVeyraDashEnd& End)
{
	UAbilitySystemComponent* Caster = Pending.Caster.Get();
	const AActor* Body = Caster ? Caster->GetAvatarActor() : nullptr;
	UWorld* World = Body ? Body->GetWorld() : nullptr;
	// Where it lands, unless a displacement cut it short (ADR-018 §6) or another move took over (ADR-031 §7).
	if (World && End.Reason != EVeyraDashEndReason::Interrupted && End.Reason != EVeyraDashEndReason::Replaced && !Pending.EndZones.IsEmpty())
	{
		FVeyraEffectFrame Landing;
		Landing.Origin = Body->GetActorLocation();
		Landing.Direction = Pending.Direction;
		Landing.bOriginIsCaster = true;
		VeyraAreaDelivery::Resolve(*World, *Caster, Landing, Pending.EndZones, Pending.Source);
	}
	// It leaves the ride as it lands, after its landing's effects, so its vehicle goes on from there and
	// spares what the landing knocked up (Roster Bible §1: one displacement per target).
	if (Caster && Pending.bLeavesRide)
	{
		VeyraCombat::EndRide(*Caster, EVeyraRideEndReason::Dismounted);
	}
	AActor* Enemy = End.Contact.Get();
	if (End.Reason != EVeyraDashEndReason::EnemyContact || !Caster || !Enemy)
	{
		return;
	}
	FVeyraEffectFrame Frame;
	Frame.Origin = Body ? Body->GetActorLocation() : Enemy->GetActorLocation();
	Frame.Direction = Pending.Direction;
	Frame.bOriginIsCaster = Body != nullptr;
	VeyraEffectDelivery::Apply(*Caster, *Enemy, Pending.Effects, Frame, Pending.Source);
	for (const FVeyraStatusSpec& Status : Pending.SelfStatuses)
	{
		VeyraCombat::ApplyStatus(*Caster, *Caster, Status);
	}
}

bool UVeyraDashAbility::IsOffensive(const FVeyraContentId& Ability) const
{
	// A dash is offensive only if what it passes through, lands on or lets go of takes its effects.
	const FVeyraDashAbilityTuning* Tuning = UVeyraAbilitiesTuningSubsystem::FindDash(Ability);
	return Tuning && (!Tuning->StartZones.IsEmpty() || !Tuning->ContactEffects.Damage.IsEmpty() || !Tuning->ContactEffects.Statuses.IsEmpty()
		|| !Tuning->ContactEffects.Displacement.IsEmpty() || !Tuning->HostEffects.Damage.IsEmpty() || !Tuning->HostEffects.Statuses.IsEmpty()
		|| !Tuning->HostEffects.Displacement.IsEmpty() || !Tuning->EndZones.IsEmpty());
}
