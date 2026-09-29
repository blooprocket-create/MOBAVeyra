// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Abilities/VeyraTargetedDamageAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Casting/VeyraCastSubsystem.h"
#include "Engine/World.h"
#include "Events/VeyraAbilityEvents.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraAbilitiesLog.h"
#include "VeyraCombatVerbs.h"

bool UVeyraTargetedDamageAbility::Defines(const FVeyraContentId& Ability) const
{
	return UVeyraAbilitiesTuningSubsystem::FindTargetedDamage(Ability) != nullptr;
}

double UVeyraTargetedDamageAbility::GetResourceCost(const FVeyraContentId& Ability, int32 /*Rank*/) const
{
	const FVeyraTargetedDamageAbilityTuning* Tuning = UVeyraAbilitiesTuningSubsystem::FindTargetedDamage(Ability);
	return Tuning ? Tuning->ResourceCost : 0.0;
}

double UVeyraTargetedDamageAbility::GetCooldownSeconds(const FVeyraContentId& Ability, int32 /*Rank*/) const
{
	const FVeyraTargetedDamageAbilityTuning* Tuning = UVeyraAbilitiesTuningSubsystem::FindTargetedDamage(Ability);
	return Tuning ? Tuning->CooldownSeconds : 0.0;
}

EVeyraCastRejection UVeyraTargetedDamageAbility::CheckTarget(const AActor& Caster, const FVeyraContentId& Ability, const FVeyraCastTarget& Target) const
{
	const FVeyraTargetedDamageAbilityTuning* Tuning = UVeyraAbilitiesTuningSubsystem::FindTargetedDamage(Ability);
	if (!Tuning)
	{
		return EVeyraCastRejection::UnknownAbility;
	}
	switch (VeyraTargeting::CheckEnemyTarget(Caster, Target.Actor, Tuning->CastRange))
	{
	case EVeyraTargetValidity::Valid:
	{
		// It may be for some kinds of unit only, as a Smite is for monsters (ADR-015 §3).
		const TOptional<EVeyraUnitKind> Kind = VeyraUnits::KindOf(Target.Actor);
		const bool bKindAllowed = Tuning->TargetKinds.IsEmpty() || (Kind.IsSet() && Tuning->TargetKinds.Contains(Kind.GetValue()));
		return bKindAllowed ? EVeyraCastRejection::None : EVeyraCastRejection::InvalidTarget;
	}
	case EVeyraTargetValidity::Dead:
		return EVeyraCastRejection::TargetDead;
	case EVeyraTargetValidity::NotHostile:
		return EVeyraCastRejection::NotHostile;
	case EVeyraTargetValidity::OutOfRange:
		return EVeyraCastRejection::OutOfRange;
	case EVeyraTargetValidity::NotVisible:
		return EVeyraCastRejection::NotVisible;
	case EVeyraTargetValidity::NotACombatant:
	case EVeyraTargetValidity::Caster:
	case EVeyraTargetValidity::Structure:
	case EVeyraTargetValidity::Ward:
		return EVeyraCastRejection::InvalidTarget;
	}
	return EVeyraCastRejection::InvalidTarget;
}

void UVeyraTargetedDamageAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const FVeyraContentId Ability = GetContentId(Handle, ActorInfo);
	const FVeyraTargetedDamageAbilityTuning* Tuning = UVeyraAbilitiesTuningSubsystem::FindTargetedDamage(Ability);
	const AActor* Caster = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	FVeyraCastTarget Target;
	Target.Actor = TriggerEventData ? const_cast<AActor*>(TriggerEventData->Target.Get()) : nullptr;

	// A targeted instant ability validates as it resolves (Combat Bible §30), and this is that moment.
	// Nothing is paid unless it commits (§26).
	if (!Tuning || !Caster || CheckTarget(*Caster, Ability, Target) != EVeyraCastRejection::None)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ true);
		return;
	}
	UAbilitySystemComponent& CasterAbilitySystem = *ActorInfo->AbilitySystemComponent;
	// It begins and commits in one moment (ADR-018 §3).
	NoteCastStarted(CasterAbilitySystem, Ability);
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ true);
		return;
	}
	NoteCastCommitted(CasterAbilitySystem, Ability);

	UAbilitySystemComponent* TargetAbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target.Actor);
	const int32 Level = GetCasterLevel(CasterAbilitySystem);
	const double Amount = VeyraAbilityRules::AtLevel(Tuning->DamageAmount, Tuning->DamagePerLevel, Level);
	bool bLanded = TargetAbilitySystem != nullptr;
	if (TargetAbilitySystem && Amount > 0.0)
	{
		FVeyraRawDamageEvent Damage;
		Damage.Components.Add({ Tuning->DamageType, Amount });
		bLanded = VeyraCombat::DealDamage(CasterAbilitySystem, *TargetAbilitySystem, Damage);
	}
	if (!bLanded)
	{
		// Commit is final (§54): the cost and cooldown stand even though the damage did not land.
		UE_LOG(LogVeyraAbilities, Warning, TEXT("%s committed %s, but its damage to %s was refused."), *GetNameSafe(Caster),
			*Ability.ToString(), *GetNameSafe(Target.Actor));
	}
	else
	{
		// Its statuses land with the hit, from the caster's Level now (Combat Bible §14's snapshot).
		for (const FVeyraContentId& StatusId : Tuning->Statuses)
		{
			if (const TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(StatusId, Level))
			{
				VeyraCombat::ApplyStatus(CasterAbilitySystem, *TargetAbilitySystem, Status.GetValue());
			}
		}
		// It resolves at once, outside the phases that issue Cast IDs, so it takes one here (Combat Bible §45).
		const UWorld* World = GetWorld();
		FVeyraAbilityHit Hit;
		Hit.Caster = &CasterAbilitySystem;
		Hit.Target = Target.Actor;
		Hit.Ability = Ability;
		Hit.CastId = World ? World->GetSubsystem<UVeyraCastSubsystem>()->IssueCastId() : 0;
		Hit.bDamaging = Amount > 0.0;
		UVeyraAbilityEventSubsystem::Announce(World, Hit);
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ false);
}
