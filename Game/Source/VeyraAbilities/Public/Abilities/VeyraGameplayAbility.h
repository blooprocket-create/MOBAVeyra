// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Abilities/GameplayAbility.h"
#include "Content/VeyraContentId.h"
#include "VeyraAbilityTypes.h"

#include "VeyraGameplayAbility.generated.h"

/**
 * The base of every Veyra ability: an archetype whose numbers come from tuning by content ID, run
 * only on the server (ADR-006 §7: no client prediction). It owns the rules every ability shares:
 * one validator for casting, an explicit Commit point that pays the cost and starts the cooldown
 * (Combat Bible §26), costs from the combatant's resource (§27), and cooldowns from the Veyra
 * ledger (ADR-006 §4). An archetype adds its target rules and its effect.
 */
UCLASS(Abstract)
class VEYRAABILITIES_API UVeyraGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UVeyraGameplayAbility();

	/**
	 * Why Caster may not cast this archetype, as content Ability, at Target now; None if it may. The
	 * one validator: VeyraAbilities::TryCast asks it before activating, and activation asks again.
	 */
	EVeyraCastRejection CheckCast(const UAbilitySystemComponent& Caster, const FVeyraContentId& Ability, const FVeyraCastTarget& Target) const;

protected:
	/** Whether this archetype's tuning defines Ability. */
	virtual bool Defines(const FVeyraContentId& Ability) const PURE_VIRTUAL(UVeyraGameplayAbility::Defines, return false;);

	/** The resource Ability costs at Commit. */
	virtual double GetResourceCost(const FVeyraContentId& Ability) const PURE_VIRTUAL(UVeyraGameplayAbility::GetResourceCost, return 0.0;);

	/** The cooldown Ability starts at Commit. */
	virtual double GetCooldownSeconds(const FVeyraContentId& Ability) const PURE_VIRTUAL(UVeyraGameplayAbility::GetCooldownSeconds, return 0.0;);

	/** The archetype's own target rules for Ability; None when Target is acceptable. */
	virtual EVeyraCastRejection CheckTarget(const AActor& Caster, const FVeyraContentId& Ability, const FVeyraCastTarget& Target) const;

	/** The content this spec runs, from the combatant's loadout. */
	FVeyraContentId GetContentId(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const;

	virtual bool CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void GetCooldownTimeRemainingAndDuration(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		float& TimeRemaining, float& CooldownDuration) const override;
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;
};
