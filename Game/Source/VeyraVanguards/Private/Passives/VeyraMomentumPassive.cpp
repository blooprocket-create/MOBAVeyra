// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Passives/VeyraMomentumPassive.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attacks/VeyraBasicAttackComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Delivery/VeyraEffectDelivery.h"
#include "Engine/World.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Movement/VeyraMovementComponent.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "Teams/VeyraTeam.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraCombatVerbs.h"

namespace VeyraMomentum
{
	// A passive has no ranks; its one amounts apply at every level.
	constexpr int32 PassiveRank = 1;
}

void UVeyraMomentumPassive::Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId)
{
	Super::Start(Owner, InPassiveId);
	const FVeyraMomentumTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindMomentum(PassiveId);
	UWorld* World = GetWorld();
	AActor* Participant = Owner.GetOwner();
	if (UVeyraBasicAttackComponent* Component = Participant ? Participant->FindComponentByClass<UVeyraBasicAttackComponent>() : nullptr)
	{
		Attacks = Component;
		HitHandle = Component->OnHit.AddUObject(this, &UVeyraMomentumPassive::OnHit);
		ModifyHandle = Component->OnModifyAttack.AddUObject(this, &UVeyraMomentumPassive::OnModifyAttack);
	}
	if (UVeyraCombatEventSubsystem* Events = World ? World->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		CastHandle = Events->OnCastCommitted.AddUObject(this, &UVeyraMomentumPassive::OnCastCommitted);
		DeathHandle = Events->OnDeath.AddUObject(this, &UVeyraMomentumPassive::OnDeath);
	}
	if (World && Tuning)
	{
		World->GetTimerManager().SetTimer(SampleTimer, this, &UVeyraMomentumPassive::Sample, static_cast<float>(Tuning->SampleSeconds), /*bLoop*/ true);
	}
}

void UVeyraMomentumPassive::Stop()
{
	if (UVeyraBasicAttackComponent* Component = Attacks.Get())
	{
		Component->OnHit.Remove(HitHandle);
		Component->OnModifyAttack.Remove(ModifyHandle);
	}
	UWorld* World = GetWorld();
	if (UVeyraCombatEventSubsystem* Events = World ? World->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		Events->OnCastCommitted.Remove(CastHandle);
		Events->OnDeath.Remove(DeathHandle);
	}
	if (World)
	{
		World->GetTimerManager().ClearTimer(SampleTimer);
	}
	Attacks.Reset();
	Super::Stop();
}

int32 UVeyraMomentumPassive::GetMomentum() const
{
	const UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const FVeyraMomentumTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindMomentum(PassiveId);
	const UVeyraStatusComponent* Statuses = Owner && Owner->GetOwner() ? Owner->GetOwner()->FindComponentByClass<UVeyraStatusComponent>() : nullptr;
	return Statuses && Tuning ? Statuses->GetStacksFrom(Tuning->Meter, *Owner) : 0;
}

int32 UVeyraMomentumPassive::GetMost() const
{
	const FVeyraMomentumTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindMomentum(PassiveId);
	const TOptional<FVeyraStatusSpec> Meter = Tuning ? UVeyraAbilitiesTuningSubsystem::FindStatus(Tuning->Meter) : TOptional<FVeyraStatusSpec>();
	return Meter.IsSet() ? Meter->MaxStacks : 0;
}

bool UVeyraMomentumPassive::IsHeldFull() const
{
	const UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const FVeyraMomentumTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindMomentum(PassiveId);
	const UVeyraStatusComponent* Statuses = Owner && Owner->GetOwner() ? Owner->GetOwner()->FindComponentByClass<UVeyraStatusComponent>() : nullptr;
	return Statuses && Tuning && Tuning->HoldFullStatuses.ContainsByPredicate([Statuses](const FVeyraContentId& Id) {
		return Statuses->GetLedger().Entries.ContainsByPredicate([&Id](const FVeyraStatusEntry& Entry) { return Entry.Id == Id; });
	});
}

void UVeyraMomentumPassive::Sample()
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const FVeyraMomentumTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindMomentum(PassiveId);
	const AActor* Body = Owner ? Owner->GetAvatarActor() : nullptr;
	UVeyraMovementComponent* Movement = Body ? Body->FindComponentByClass<UVeyraMovementComponent>() : nullptr;
	if (!Tuning || !Movement || !VeyraTargeting::IsAlive(Body))
	{
		return;
	}
	// By the distance she moves herself; standing still builds none, mounted or not (§1).
	Carried += Movement->TakeTravelled();
	const int32 Points = FMath::FloorToInt(Carried / Tuning->UnitsPerPoint);
	Carried -= Points * Tuning->UnitsPerPoint;
	// Held full, as NO BRAKES holds it.
	AddPoints(IsHeldFull() ? GetMost() - GetMomentum() : Points);
	MaybeRedline();
}

void UVeyraMomentumPassive::OnHit(const FVeyraAttackEvent& Event)
{
	const FVeyraMomentumTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindMomentum(PassiveId);
	if (Tuning && Event.Attacker.Get() == OwnerAbilitySystem.Get())
	{
		AddPoints(Tuning->PointsPerAttack);
	}
}

void UVeyraMomentumPassive::OnCastCommitted(const FVeyraCastEvent& Event)
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const FVeyraMomentumTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindMomentum(PassiveId);
	if (!Owner || !Tuning || Event.Caster.Get() != Owner)
	{
		return;
	}
	const bool bRedlinedCast = Tuning->Redlined.ContainsByPredicate([&Event](const FVeyraRedlinedTuning& Form) { return Form.Ability == Event.Ability; });
	if (!bRedlinedCast)
	{
		AddPoints(Tuning->PointsPerCast);
		return;
	}
	// A Redlined cast spends the meter, its used-once forms leave their slots together, and Roadhouse waits (§1).
	VeyraCombat::RemoveStatus(*Owner, Tuning->Meter);
	bRedlined = false;
	bRoadhouse = true;
	if (TOptional<FVeyraStatusSpec> Reach = UVeyraAbilitiesTuningSubsystem::FindStatus(Tuning->Roadhouse.ReachStatus))
	{
		VeyraCombat::ApplyStatus(*Owner, *Owner, Reach.GetValue());
	}
}

void UVeyraMomentumPassive::OnModifyAttack(FVeyraAttackPlan& Plan)
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const FVeyraMomentumTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindMomentum(PassiveId);
	AActor* Target = Plan.Target.Get();
	const AActor* Body = Owner ? Owner->GetAvatarActor() : nullptr;
	UAbilitySystemComponent* Struck = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!bRoadhouse || !Tuning || !Owner || !Body || !Target || !Struck || !VeyraUnits::IsVanguard(Target) || !VeyraTargeting::AreHostile(Body, Target))
	{
		return;
	}
	// Roadhouse: bonus damage from the target's missing Health and her bonus Health, and a lunge to it (§1).
	const FVeyraRoadhouseTuning& Roadhouse = Tuning->Roadhouse;
	const double BonusHealth = FMath::Max(0.0, Owner->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute())
		- Owner->GetNumericAttributeBase(UVeyraVitalsSet::GetMaxHealthAttribute()));
	Plan.AddDamage(Roadhouse.Damage.Type, VeyraEffectDelivery::DamageAmount(*Owner, Roadhouse.Damage, VeyraMomentum::PassiveRank)
		+ VeyraCombat::GetMissingHealth(*Struck) * Roadhouse.MissingHealthRatio + BonusHealth * Roadhouse.BonusHealthRatio);
	bRoadhouse = false;
	VeyraCombat::RemoveStatus(*Owner, Roadhouse.ReachStatus);
	const double Gap = VeyraTargeting::EdgeToEdgeDistance(*Body, *Target);
	const FVector Toward = (Target->GetActorLocation() - Body->GetActorLocation()).GetSafeNormal2D();
	if (Gap > 0.0 && !Toward.IsNearlyZero())
	{
		VeyraCombat::Dash(*Owner, FVeyraDash{ Toward, Gap, Roadhouse.LungeSpeed, EVeyraDashContact::None });
	}
}

void UVeyraMomentumPassive::OnDeath(const FVeyraDeathEvent& Death)
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	if (!Owner || Death.Victim.Get() != Owner)
	{
		return;
	}
	// It resets on death (ADR-018 §8): the meter goes with her statuses, Redline and Roadhouse with it.
	if (UVeyraAbilityLoadoutComponent* Loadout = Owner->GetOwner() ? Owner->GetOwner()->FindComponentByClass<UVeyraAbilityLoadoutComponent>() : nullptr)
	{
		Loadout->EndGroup(*Owner, RedlineGroup());
	}
	bRedlined = false;
	bRoadhouse = false;
	Carried = 0.0;
}

void UVeyraMomentumPassive::AddPoints(int32 Points)
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const FVeyraMomentumTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindMomentum(PassiveId);
	const TOptional<FVeyraStatusSpec> Meter = Tuning ? UVeyraAbilitiesTuningSubsystem::FindStatus(Tuning->Meter) : TOptional<FVeyraStatusSpec>();
	if (!Owner || !Meter.IsSet() || Points <= 0 || !VeyraTargeting::IsAlive(Owner->GetAvatarActor()))
	{
		return;
	}
	const int32 Room = FMath::Max(0, GetMost() - GetMomentum());
	for (int32 Added = 0; Added < FMath::Min(Points, Room); ++Added)
	{
		VeyraCombat::ApplyStatus(*Owner, *Owner, Meter.GetValue());
	}
	MaybeRedline();
}

void UVeyraMomentumPassive::MaybeRedline()
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const FVeyraMomentumTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindMomentum(PassiveId);
	// A ride's mounted set has no Redlined forms, so Redline waits for her to leave it.
	if (bRedlined || !Owner || !Tuning || GetMomentum() < GetMost() || GetMost() <= 0 || VeyraCombat::IsRiding(*Owner))
	{
		return;
	}
	UVeyraAbilityLoadoutComponent* Loadout = Owner->GetOwner() ? Owner->GetOwner()->FindComponentByClass<UVeyraAbilityLoadoutComponent>() : nullptr;
	if (!Loadout)
	{
		return;
	}
	for (const FVeyraRedlinedTuning& Form : Tuning->Redlined)
	{
		FVeyraOverrideSpec Spec;
		Spec.Ability = Form.Ability;
		Spec.Use = EVeyraOverrideUse::Once;
		Spec.Group = RedlineGroup();
		// A Redlined form is its slot's own ability run harder: one cooldown for both.
		Spec.bSharesCooldown = true;
		Loadout->Override(*Owner, Form.Slot, Spec);
	}
	bRedlined = true;
}

FName UVeyraMomentumPassive::RedlineGroup() const
{
	return FName(*FString::Printf(TEXT("redline_%s"), *PassiveId.ToString()));
}
