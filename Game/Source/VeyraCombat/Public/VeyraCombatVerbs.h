// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Absorption/VeyraAbsorptionLedger.h"
#include "ActiveGameplayEffectHandle.h"
#include "Damage/VeyraDamageTypes.h"

class UAbilitySystemComponent;
class UVeyraDamageAbsorptionComponent;

/**
 * Combat's verbs (ARCHITECTURE.md §1.10): the one way gameplay code deals damage, grants shields and
 * Temporary Health, and prepares a unit for combat. Every amount and duration comes from the
 * caller's validated data; these functions never supply numbers of their own. Server only.
 */
namespace VeyraCombat
{
	/**
	 * Prepares a unit's Ability System Component for combat: installs the §41 modifier policy and
	 * connects its absorption component. Call once per unit.
	 */
	VEYRACOMBAT_API void ConfigureCombatant(UAbilitySystemComponent& AbilitySystem, UVeyraDamageAbsorptionComponent& Absorption);

	/** Sets a unit's base Max Health from its data and fills its Health. Returns false if refused. */
	VEYRACOMBAT_API bool InitializeVitals(UAbilitySystemComponent& AbilitySystem, double MaxHealth);

	/**
	 * Deals one damage event from Source to Target through the canonical pipeline (Combat Bible §25).
	 * Each damage type may appear once, with a finite amount of at least 0. Returns false if refused.
	 */
	VEYRACOMBAT_API bool DealDamage(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target, const FVeyraRawDamageEvent& Damage);

	/** Grants Target a shield (Combat Bible §7). Returns its effect, or an invalid handle if refused. */
	VEYRACOMBAT_API FActiveGameplayEffectHandle GrantShield(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target,
		EVeyraShieldCategory Category, double Amount, double DurationSeconds);

	/** Grants Target Temporary Health (Combat Bible §7). Returns its effect, or an invalid handle if refused. */
	VEYRACOMBAT_API FActiveGameplayEffectHandle GrantTemporaryHealth(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target,
		double Amount, double DurationSeconds);
}
