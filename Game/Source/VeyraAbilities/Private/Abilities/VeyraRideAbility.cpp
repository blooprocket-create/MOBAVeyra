// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Abilities/VeyraRideAbility.h"

#include "AbilitySystemComponent.h"
#include "Delivery/VeyraEffectDelivery.h"
#include "Delivery/VeyraProjectile.h"
#include "Engine/World.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Movement/VeyraMovementComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Units/VeyraUnit.h"
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
	// A newer ride replaces the older, whichever ability began it: the older ends as any ride ends, its crash, its
	// statuses, its mounted actions and its time with it, so nothing of it outlasts the change.
	if (VeyraCombat::IsRiding(*Caster))
	{
		VeyraCombat::EndRide(*Caster, EVeyraRideEndReason::Replaced);
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
	// After any older ride's end, which crashed with its own.
	CrashZones = VeyraAreaDelivery::PrepareZones(*Caster, Tuning->CrashZones, Cast.Rank);
	// What its body does to what it meets, from the rider's rank and power now (ADR-035 §6).
	if (!Tuning->Contact.IsEmpty())
	{
		const FVeyraRideContactTuning& Contact = Tuning->Contact[0];
		ContactEffects = VeyraEffectDelivery::Prepare(*Caster, Contact.EnemyEffects, Cast.Rank);
		if (!Contact.AllyEffects.IsEmpty())
		{
			ContactHelp = VeyraAreaDelivery::PrepareAllyEffects(*Caster, Contact.AllyEffects[0], Cast.Rank);
		}
	}
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
	// Its contact and its trail look about them as it sets off, then on their pulses (ADR-035 §6).
	if (!Tuning->Contact.IsEmpty())
	{
		World->GetTimerManager().SetTimer(ContactTimer, FTimerDelegate::CreateWeakLambda(Caster, [Self]() {
			if (UVeyraRideAbility* Ability = Self.Get())
			{
				Ability->PulseContact();
			}
		}), static_cast<float>(Tuning->Contact[0].PulseSeconds), /*bLoop*/ true);
		PulseContact();
	}
	if (!Tuning->Trail.IsEmpty())
	{
		TrailFrom = Body->GetActorLocation();
		TrailTravelled = Tuning->Trail[0].Spacing;
		World->GetTimerManager().SetTimer(TrailTimer, FTimerDelegate::CreateWeakLambda(Caster, [Self]() {
			if (UVeyraRideAbility* Ability = Self.Get())
			{
				Ability->PulseTrail();
			}
		}), static_cast<float>(Tuning->Trail[0].PulseSeconds), /*bLoop*/ true);
		PulseTrail();
	}
	return FVeyraChannelPlan();
}

void UVeyraRideAbility::PulseContact()
{
	UAbilitySystemComponent* Caster = Rider.Get();
	const FVeyraRideAbilityTuning* Tuning = UVeyraAbilitiesTuningSubsystem::FindRide(RideAbility);
	const AActor* Body = Caster ? Caster->GetAvatarActor() : nullptr;
	UWorld* World = GetWorld();
	if (!Caster || !Tuning || Tuning->Contact.IsEmpty() || !Body || !World || !ContactEffects.IsSet())
	{
		return;
	}
	const FVeyraRideContactTuning& Contact = Tuning->Contact[0];
	// Struck along the ride's line, so a knock aside pushes off it.
	const UVeyraMovementComponent* Movement = Watched.Get();
	FVeyraEffectFrame Frame;
	Frame.Origin = Body->GetActorLocation();
	Frame.Direction = Movement ? Movement->GetRideHeading() : Body->GetActorForwardVector().GetSafeNormal2D();
	FVeyraShape Touch;
	Touch.Kind = EVeyraShapeKind::Circle;
	Touch.Radius = Body->GetSimpleCollisionRadius() + Contact.Reach;
	const FVeyraAbilityHitSource Source{ RideAbility, RideCastId };
	const TArray<AActor*> Units = VeyraShapes::GatherUnits(*World, FVeyraPlacedShape{ Touch, Frame.Origin, Frame.Direction }, [this, Body](const AActor& Unit) {
		return &Unit != Body && !Met.ContainsByPredicate([&Unit](const TWeakObjectPtr<AActor>& Earlier) { return Earlier.Get() == &Unit; });
	});
	for (AActor* Unit : Units)
	{
		const TOptional<EVeyraUnitKind> Kind = VeyraUnits::KindOf(Unit);
		const bool bStruck = Kind.IsSet() && (Contact.EnemyKinds.IsEmpty() || Contact.EnemyKinds.Contains(Kind.GetValue()))
			&& Kind.GetValue() != EVeyraUnitKind::Structure && Kind.GetValue() != EVeyraUnitKind::Ward;
		if (bStruck && VeyraTargeting::CanHitEnemy(Caster->GetOwner(), *Unit) && VeyraTargeting::IsAlive(Unit))
		{
			Met.Add(Unit);
			VeyraEffectDelivery::Apply(*Caster, *Unit, ContactEffects.GetValue(), Frame, Source);
		}
		else if (ContactHelp.IsSet() && VeyraAreaDelivery::Reaches(*Caster, *Unit, ContactHelp.GetValue()))
		{
			Met.Add(Unit);
			VeyraAreaDelivery::HelpAlly(*Caster, *Unit, ContactHelp.GetValue());
		}
	}
}

void UVeyraRideAbility::PulseTrail()
{
	UAbilitySystemComponent* Caster = Rider.Get();
	const FVeyraRideAbilityTuning* Tuning = UVeyraAbilitiesTuningSubsystem::FindRide(RideAbility);
	const AActor* Body = Caster ? Caster->GetAvatarActor() : nullptr;
	UWorld* World = GetWorld();
	if (!Caster || !Tuning || Tuning->Trail.IsEmpty() || !Body || !World)
	{
		return;
	}
	const FVeyraRideTrailTuning& Trail = Tuning->Trail[0];
	const FVeyraAreaAbilityTuning* Area = UVeyraAbilitiesTuningSubsystem::FindArea(Trail.Area);
	const FVector From = TrailFrom;
	const FVector Here = Body->GetActorLocation();
	const double Stride = FVector::Dist2D(Here, From);
	TrailFrom = Here;
	if (!Area)
	{
		return;
	}
	// An area every spacing along the way it went, where that spacing falls, what is left over carried into the
	// next look: its spacing holds at any speed and pulse.
	const UVeyraMovementComponent* Movement = Watched.Get();
	const FVector Heading = Movement ? Movement->GetRideHeading() : Body->GetActorForwardVector().GetSafeNormal2D();
	double Along = Trail.Spacing - TrailTravelled;
	TrailTravelled += Stride;
	for (; TrailTravelled >= Trail.Spacing; TrailTravelled -= Trail.Spacing, Along += Trail.Spacing)
	{
		FVeyraEffectFrame Placement;
		Placement.Origin = Stride > 0.0 ? FMath::Lerp(From, Here, FMath::Clamp(Along / Stride, 0.0, 1.0)) : Here;
		Placement.Direction = Heading;
		const TArray<FVeyraPreparedZone> Zones = VeyraAreaDelivery::PrepareZones(*Caster, Area->Zones, RideRank);
		VeyraAreaDelivery::Resolve(*World, *Caster, Placement, Zones, FVeyraAbilityHitSource{ Trail.Area, RideCastId });
		if (const TOptional<FVeyraPreparedLinger> Linger = VeyraAreaDelivery::PrepareLinger(*Caster, *Area, RideRank, GetCasterLevel(*Caster), Trail.Area, RideCastId))
		{
			VeyraAreaDelivery::ArmLinger(*World, *Caster, Placement, Linger.GetValue());
		}
	}
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
	if (Holding && Holding->Ability == CastTuning->RecastWindow[0].Ability && Caster->GetAvatarActor())
	{
		const AActor* Body = Caster->GetAvatarActor();
		const UVeyraMovementComponent* Movement = Watched.Get();
		FVeyraCastTarget Ahead;
		Ahead.bHasLocation = Body != nullptr;
		Ahead.Location = Body ? Body->GetActorLocation() + (Movement ? Movement->GetRideHeading() : Body->GetActorForwardVector()) * Body->GetSimpleCollisionRadius() * 4.0
			: FVector::ZeroVector;
		// It leaves the ride as it lands (validation holds it to a dash that leaves), and ends it then.
		if (VeyraAbilities::TryCast(*Caster, Holding->Slot, Ahead) == EVeyraCastRejection::None)
		{
			return;
		}
	}
	if (!Rider.IsValid() || !VeyraCombat::IsRiding(*Caster))
	{
		return;
	}
	// Mid-dash, as when its recast fired on its own timer: the dash ends first. One that leaves the
	// ride ends it as it lands, after its landing; any other hands the end back to the clock.
	UVeyraMovementComponent* Movement = Watched.Get();
	if (Movement && Movement->IsDashing())
	{
		if (!ExpiryDashHandle.IsValid())
		{
			ExpiryDashHandle = Movement->OnDashEnded.AddWeakLambda(this, [this](const FVeyraDashEnd&) {
				// On the next tick, after the dash's own landing, which the same event runs (listeners run
				// newest first).
				UAbilitySystemComponent* Still = Rider.Get();
				UWorld* World = GetWorld();
				if (Still && World)
				{
					World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(Still, [Held = TWeakObjectPtr<UAbilitySystemComponent>(Still)]() {
						if (UAbilitySystemComponent* Unit = Held.Get(); Unit && VeyraCombat::IsRiding(*Unit))
						{
							VeyraCombat::EndRide(*Unit, EVeyraRideEndReason::Expired);
						}
					}));
				}
			});
		}
		return;
	}
	VeyraCombat::EndRide(*Caster, EVeyraRideEndReason::Expired);
}

void UVeyraRideAbility::OnRideEnded(const FVeyraRideEnd& End)
{
	UAbilitySystemComponent* Caster = Rider.Get();
	const FVeyraContentId Ability = RideAbility;
	const int32 CastId = RideCastId;
	const FName Group = MountedGroup();
	const TArray<FVeyraPreparedZone> Crash = MoveTemp(CrashZones);
	CrashZones.Reset();
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
	// Its crash erupts where its rider is, as the rider's hit, on every end but death (ADR-035 §3).
	UWorld* World = GetWorld();
	if (End.Reason != EVeyraRideEndReason::Died && !Crash.IsEmpty() && World)
	{
		// A crash that strikes is an attack, whatever set it off: its rider is seen (Combat Bible §11).
		const bool bStrikes = Tuning->CrashZones.ContainsByPredicate([](const FVeyraAreaZoneTuning& Zone) {
			return !Zone.Effects.Damage.IsEmpty() || !Zone.Effects.Statuses.IsEmpty() || !Zone.Effects.Displacement.IsEmpty() || !Zone.Effects.Reactions.IsEmpty();
		});
		if (bStrikes)
		{
			VeyraCombat::EndStealth(*Caster);
		}
		FVeyraEffectFrame Frame;
		Frame.Origin = End.Location;
		Frame.Direction = End.Heading;
		VeyraAreaDelivery::Resolve(*World, *Caster, Frame, Crash, FVeyraAbilityHitSource{ Ability, CastId });
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
		Movement->OnDashEnded.Remove(ExpiryDashHandle);
	}
	ExpiryDashHandle.Reset();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ExpiryTimer);
		World->GetTimerManager().ClearTimer(ContactTimer);
		World->GetTimerManager().ClearTimer(TrailTimer);
		if (UVeyraCombatEventSubsystem* Events = World->GetSubsystem<UVeyraCombatEventSubsystem>())
		{
			Events->OnDeath.Remove(DeathHandle);
		}
	}
	Watched.Reset();
	RideEndedHandle.Reset();
	DeathHandle.Reset();
	Rider.Reset();
	ContactEffects.Reset();
	ContactHelp.Reset();
	Met.Reset();
}
