// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Abilities/VeyraTargetedDamageAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraAbilitiesLog.h"
#include "VeyraCombatVerbs.h"

bool UVeyraTargetedDamageAbility::Defines(const FVeyraContentId& Ability) const
{
	return UVeyraAbilitiesTuningSubsystem::FindTargetedDamage(Ability) != nullptr;
}

double UVeyraTargetedDamageAbility::GetResourceCost(const FVeyraContentId& Ability) const
{
	const FVeyraTargetedDamageAbilityTuning* Tuning = UVeyraAbilitiesTuningSubsystem::FindTargetedDamage(Ability);
	return Tuning ? Tuning->ResourceCost : 0.0;
}

double UVeyraTargetedDamageAbility::GetCooldownSeconds(const FVeyraContentId& Ability) const
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
		return EVeyraCastRejection::None;
	case EVeyraTargetValidity::Dead:
		return EVeyraCastRejection::TargetDead;
	case EVeyraTargetValidity::NotHostile:
		return EVeyraCastRejection::NotHostile;
	case EVeyraTargetValidity::OutOfRange:
		return EVeyraCastRejection::OutOfRange;
	case EVeyraTargetValidity::NotACombatant:
	case EVeyraTargetValidity::Caster:
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
	if (!Tuning || !Caster || CheckTarget(*Caster, Ability, Target) != EVeyraCastRejection::None || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ true);
		return;
	}

	UAbilitySystemComponent* TargetAbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target.Actor);
	FVeyraRawDamageEvent Damage;
	Damage.Components.Add({ Tuning->DamageType, Tuning->DamageAmount });
	if (!TargetAbilitySystem || !VeyraCombat::DealDamage(*ActorInfo->AbilitySystemComponent, *TargetAbilitySystem, Damage))
	{
		// Commit is final (§54): the cost and cooldown stand even though the damage did not land.
		UE_LOG(LogVeyraAbilities, Warning, TEXT("%s committed %s, but its damage to %s was refused."), *GetNameSafe(Caster),
			*Ability.ToString(), *GetNameSafe(Target.Actor));
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ false);
}
