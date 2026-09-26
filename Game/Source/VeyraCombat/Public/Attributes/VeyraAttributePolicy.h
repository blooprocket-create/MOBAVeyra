// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "AttributeSet.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Misc/Optional.h"

class UGameplayEffect;
struct FActiveGameplayEffectsContainer;
struct FGameplayEffectSpec;

/**
 * How Gameplay Effects may change a Veyra attribute (Combat Bible §41, ADR-006 §4). The Gameplay
 * Ability System sums percentage modifiers by default; Veyra's must multiply, so every percentage
 * is a MultiplyCompound factor (1 + x for a bonus, 1 − x for a reduction) and every flat change is
 * AddBase. The result is (base + flat) × Π factors, the §41 order.
 */
enum class EVeyraModifierRule : uint8
{
	/** No effect modifies it: pools and meta attributes change only through their owner. */
	None,
	/** Flat changes only (AddBase). */
	Flat,
	/** Percentage factors only (MultiplyCompound). */
	Percentage,
	/** A §41 stat: flat changes and percentage factors. */
	Stat,
};

namespace VeyraAttributePolicy
{
	/** The rule for a Veyra attribute; unset for any attribute the policy does not know. */
	VEYRACOMBAT_API TOptional<EVeyraModifierRule> RuleFor(const FGameplayAttribute& Attribute);

	/**
	 * Why an effect definition breaks the policy, or nothing when it is allowed. Modifiers must use
	 * their attribute's operations and must belong to a duration effect: an instant or periodic
	 * effect would change the base value for good and break the §41 order. Only Veyra's own
	 * executions may run.
	 */
	VEYRACOMBAT_API TArray<FString> Check(const UGameplayEffect& Effect);

	/** The application query every combatant installs: logs and rejects effects that break the policy. */
	VEYRACOMBAT_API bool AllowsSpec(const FActiveGameplayEffectsContainer& ActiveEffects, const FGameplayEffectSpec& Spec);
}
