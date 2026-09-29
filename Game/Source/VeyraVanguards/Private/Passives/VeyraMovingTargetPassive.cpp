// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Passives/VeyraMovingTargetPassive.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attacks/VeyraBasicAttackComponent.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Delivery/VeyraEffectDelivery.h"
#include "Engine/World.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Teams/VeyraTeam.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraCombatVerbs.h"

namespace VeyraMovingTarget
{
	// A passive has no ranks; its one amounts apply at every level.
	constexpr int32 PassiveRank = 1;
	// Dead Reckoning's ratio is per this many units banked (Roster Bible §2's "per 100 units").
	constexpr double UnitsPerStep = 100.0;
}

void UVeyraMovingTargetPassive::Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId)
{
	Super::Start(Owner, InPassiveId);
	AActor* Participant = Owner.GetOwner();
	if (UVeyraBasicAttackComponent* Component = Participant ? Participant->FindComponentByClass<UVeyraBasicAttackComponent>() : nullptr)
	{
		Attacks = Component;
		ModifyHandle = Component->OnModifyAttack.AddUObject(this, &UVeyraMovingTargetPassive::OnModifyAttack);
	}
	if (UVeyraCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		DisplacedHandle = Events->OnDisplaced.AddUObject(this, &UVeyraMovingTargetPassive::OnDisplaced);
	}
}

void UVeyraMovingTargetPassive::Stop()
{
	if (UVeyraBasicAttackComponent* Component = Attacks.Get())
	{
		Component->OnModifyAttack.Remove(ModifyHandle);
	}
	if (UVeyraCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		Events->OnDisplaced.Remove(DisplacedHandle);
	}
	ModifyHandle.Reset();
	DisplacedHandle.Reset();
	Attacks.Reset();
	Super::Stop();
}

void UVeyraMovingTargetPassive::OnDisplaced(const FVeyraDisplacementEvent& Event)
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const FVeyraMovingTargetTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindMovingTarget(PassiveId);
	const UAbilitySystemComponent* Source = Event.Source.Get();
	UAbilitySystemComponent* Target = Event.Target.Get();
	if (!Owner || !Tuning || !Source || !Target)
	{
		return;
	}
	// An enemy Vanguard that Kade or an ally moved (§2). His own dashes are not displacement.
	const EVeyraTeam Side = VeyraTeams::TeamOf(Owner->GetOwner());
	if (VeyraTeams::TeamOf(Source->GetOwner()) != Side || VeyraTeams::TeamOf(Target->GetOwner()) == Side || !VeyraUnits::IsVanguard(Target->GetAvatarActor()))
	{
		return;
	}
	const UVeyraProgressionComponent* Progression = Owner->GetOwner()->FindComponentByClass<UVeyraProgressionComponent>();
	const int32 Level = Progression && Progression->IsInitialized() ? Progression->GetLevel() : 1;
	if (const TOptional<FVeyraStatusSpec> Tracked = UVeyraAbilitiesTuningSubsystem::FindStatus(Tuning->TrackedStatus, Level))
	{
		VeyraCombat::ApplyStatus(*Owner, *Target, Tracked.GetValue());
	}
	Banked = FMath::Min(Banked + Event.Distance, Tuning->DeadReckoning.CapUnits);
}

void UVeyraMovingTargetPassive::OnModifyAttack(FVeyraAttackPlan& Plan)
{
	const UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const FVeyraMovingTargetTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindMovingTarget(PassiveId);
	const UAbilitySystemComponent* Target = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Plan.Target.Get());
	const UVeyraStatusComponent* Marks = Target && Target->GetOwner() ? Target->GetOwner()->FindComponentByClass<UVeyraStatusComponent>() : nullptr;
	if (!Owner || !Tuning || !Marks || !Marks->HasFrom(Tuning->TrackedStatus, *Owner))
	{
		return;
	}
	Plan.AddDamage(Tuning->TrackedDamage.Type, VeyraEffectDelivery::DamageAmount(*Owner, Tuning->TrackedDamage, VeyraMovingTarget::PassiveRank));
	// Dead Reckoning: what is banked past the threshold empowers this attack, and is spent.
	const FVeyraDeadReckoningTuning& Reckoning = Tuning->DeadReckoning;
	if (Banked >= Reckoning.ThresholdUnits)
	{
		const double Power = Owner->GetNumericAttribute(UVeyraOffenceSet::GetPhysicalPowerAttribute());
		const double Amount = VeyraEffectDelivery::DamageAmount(*Owner, Reckoning.Damage, VeyraMovingTarget::PassiveRank)
			+ Power * Reckoning.PhysicalPowerRatioPerHundredUnits * (Banked / VeyraMovingTarget::UnitsPerStep);
		Plan.AddDamage(Reckoning.Damage.Type, Amount);
		Banked = 0.0;
	}
}
