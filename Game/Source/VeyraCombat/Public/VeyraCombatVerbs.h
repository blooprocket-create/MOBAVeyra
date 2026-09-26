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

	/** Sets a unit's base Move Speed from its data. Returns false if refused. */
	VEYRACOMBAT_API bool InitializeMoveSpeed(UAbilitySystemComponent& AbilitySystem, double MoveSpeed);

	/**
	 * Sets a unit's base Max Resource from its data and fills its Resource (Combat Bible §27). 0 means
	 * the unit has no resource. Returns false if refused.
	 */
	VEYRACOMBAT_API bool InitializeResource(UAbilitySystemComponent& AbilitySystem, double MaxResource);

	/** Whether the unit has at least Amount of its resource. A cost of 0 is always affordable. */
	VEYRACOMBAT_API bool CanAffordResource(const UAbilitySystemComponent& AbilitySystem, double Amount);

	/**
	 * Pays Amount of the unit's resource (Combat Bible §27). Refused, changing nothing, if the unit
	 * cannot afford it: resources never go negative. Returns false if refused.
	 */
	VEYRACOMBAT_API bool SpendResource(UAbilitySystemComponent& AbilitySystem, double Amount);

	/**
	 * Brings a dead unit back for its respawn: alive, with full Health and Resource (Combat Bible
	 * §18). Returns false if the unit was not dead.
	 */
	VEYRACOMBAT_API bool Revive(UAbilitySystemComponent& AbilitySystem);

	/**
	 * Deals one damage event from Source to Target through the canonical pipeline (Combat Bible §25).
	 * Each damage type may appear once, with a finite amount of at least 0. A target whose death is
	 * final takes no damage. Returns false if refused.
	 */
	VEYRACOMBAT_API bool DealDamage(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target, const FVeyraRawDamageEvent& Damage);

	/** Grants Target a shield (Combat Bible §7). Returns its effect, or an invalid handle if refused. */
	VEYRACOMBAT_API FActiveGameplayEffectHandle GrantShield(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target,
		EVeyraShieldCategory Category, double Amount, double DurationSeconds);

	/** Grants Target Temporary Health (Combat Bible §7). Returns its effect, or an invalid handle if refused. */
	VEYRACOMBAT_API FActiveGameplayEffectHandle GrantTemporaryHealth(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target,
		double Amount, double DurationSeconds);
}
