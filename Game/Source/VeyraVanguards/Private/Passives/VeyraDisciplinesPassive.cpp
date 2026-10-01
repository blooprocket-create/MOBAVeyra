// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Passives/VeyraDisciplinesPassive.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Engine/World.h"
#include "Events/VeyraAbilityEvents.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraAbilitiesVerbs.h"
#include "VeyraCombatVerbs.h"
#include "VeyraVanguardsLog.h"

void UVeyraDisciplinesPassive::Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId)
{
	Super::Start(Owner, InPassiveId);
	if (UWorld* World = GetWorld())
	{
		if (UVeyraAbilityEventSubsystem* Events = World->GetSubsystem<UVeyraAbilityEventSubsystem>())
		{
			HitHandle = Events->OnAbilityHit.AddUObject(this, &UVeyraDisciplinesPassive::OnAbilityHit);
		}
	}
}

void UVeyraDisciplinesPassive::Stop()
{
	if (UWorld* World = GetWorld())
	{
		if (UVeyraAbilityEventSubsystem* Events = World->GetSubsystem<UVeyraAbilityEventSubsystem>())
		{
			Events->OnAbilityHit.Remove(HitHandle);
		}
	}
	HitHandle.Reset();
	Super::Stop();
}

void UVeyraDisciplinesPassive::OnAbilityHit(const FVeyraAbilityHit& Hit)
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	AActor* Target = Hit.Target.Get();
	const FVeyraDisciplinesTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindDisciplines(PassiveId);
	if (!Owner || !Target || !Tuning || Hit.Caster.Get() != Owner)
	{
		return;
	}
	// Marks are Vanguard terms, spent only on enemies (ADR-031 §10).
	UAbilitySystemComponent* Held = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!Held || !VeyraUnits::IsVanguard(Target) || !VeyraTargeting::AreHostile(Owner->GetOwner(), Target))
	{
		return;
	}
	for (const FVeyraDisciplineMarkTuning& Mark : Tuning->Marks)
	{
		if (!Mark.ConsumedBy.Contains(Hit.Ability) || !VeyraCombat::HasStatusFrom(Target, Mark.Status, *Owner))
		{
			continue;
		}
		VeyraCombat::RemoveStatus(*Held, Mark.Status);
		const FVeyraDisciplineBonusTuning* Bonus = Tuning->Bonuses.FindByPredicate([&Hit](const FVeyraDisciplineBonusTuning& Candidate) { return Candidate.Ability == Hit.Ability; });
		Strike(*Owner, *Held, Mark, Bonus ? Bonus->DamageMultiplier : 1.0);
		if (Mark.ResourceRefund > 0.0)
		{
			VeyraCombat::RestoreResource(*Owner, VeyraAbilities::ResourceCostOf(*Owner, Hit.Ability) * Mark.ResourceRefund);
		}
		for (const FVeyraContentId& StatusId : Mark.CasterStatuses)
		{
			if (const TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(StatusId))
			{
				VeyraCombat::ApplyStatus(*Owner, *Owner, Status.GetValue());
			}
		}
		if (Bonus && Bonus->CooldownRefund > 0.0)
		{
			VeyraAbilities::RefundCooldown(*Owner, Hit.Ability, Bonus->CooldownRefund);
		}
		VeyraAbilities::ShortenCooldown(*Owner, Tuning->RefundSlot, Tuning->RefundSeconds);
		UE_LOG(LogVeyraVanguards, Verbose, TEXT("%s's %s spends %s on %s."), *GetNameSafe(Owner->GetOwner()), *Hit.Ability.ToString(), *Mark.Status.ToString(),
			*GetNameSafe(Target));
	}
}

void UVeyraDisciplinesPassive::Strike(UAbilitySystemComponent& Owner, UAbilitySystemComponent& Target, const FVeyraDisciplineMarkTuning& Mark, double Multiplier)
{
	const UVeyraProgressionComponent* Progression = Owner.GetOwner() ? Owner.GetOwner()->FindComponentByClass<UVeyraProgressionComponent>() : nullptr;
	const int32 Level = Progression && Progression->IsInitialized() ? Progression->GetLevel() : 1;
	const double Power = Owner.GetNumericAttribute(UVeyraOffenceSet::GetPhysicalPowerAttribute());
	const double Amount = (Mark.DamageAmount + Mark.DamagePerLevel * (Level - 1) + Mark.PhysicalPowerRatio * Power) * Multiplier;
	if (!(Amount > 0.0))
	{
		return;
	}
	FVeyraRawDamageEvent Event;
	Event.Components.Add({ Mark.DamageType, Amount });
	// It ignores part of the resistance its type meets (ADR-031 §10).
	FVeyraPenetration& Penetration = Mark.DamageType == EVeyraDamageType::Magic ? Event.MagicPenetration : Event.PhysicalPenetration;
	Penetration.Retained = 1.0 - Mark.Penetration;
	VeyraCombat::DealDamage(Owner, Target, Event);
}
