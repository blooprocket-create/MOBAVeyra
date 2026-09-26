// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Abilities/VeyraGameplayAbility.h"

#include "VeyraTargetedDamageAbility.generated.h"

/**
 * The archetype for an ability cast at one enemy that resolves at once and deals one damage
 * component (Combat Bible §29: targeted, instant). Each ability of this kind is an entry in
 * Abilities.json's targetedDamage map; the developer test ability test_bolt is one.
 */
UCLASS()
class VEYRAABILITIES_API UVeyraTargetedDamageAbility : public UVeyraGameplayAbility
{
	GENERATED_BODY()

protected:
	virtual bool Defines(const FVeyraContentId& Ability) const override;
	virtual double GetResourceCost(const FVeyraContentId& Ability) const override;
	virtual double GetCooldownSeconds(const FVeyraContentId& Ability) const override;
	virtual EVeyraCastRejection CheckTarget(const AActor& Caster, const FVeyraContentId& Ability, const FVeyraCastTarget& Target) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
