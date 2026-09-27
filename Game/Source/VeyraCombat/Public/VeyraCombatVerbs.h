// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Absorption/VeyraAbsorptionLedger.h"
#include "ActiveGameplayEffectHandle.h"
#include "Damage/VeyraDamageTypes.h"
#include "GameplayEffectTypes.h"
#include "Stats/VeyraStatBlock.h"
#include "Statuses/VeyraStatusTypes.h"

class UAbilitySystemComponent;
class UVeyraDamageAbsorptionComponent;
class UVeyraStatusComponent;

/**
 * A damage event prepared at Commit (Combat Bible §50, ADR-009 §4), which a projectile or delayed
 * area carries until it lands. Server only.
 */
struct FVeyraPreparedDamage
{
	FGameplayEffectSpecHandle Spec;

	bool IsValid() const { return Spec.IsValid(); }
};

/**
 * Combat's verbs (ARCHITECTURE.md §1.10): the one way gameplay code deals damage, grants shields and
 * Temporary Health, and prepares a unit for combat. Every amount and duration comes from the
 * caller's validated data; these functions never supply numbers of their own. Server only.
 */
namespace VeyraCombat
{
	/**
	 * Prepares a unit's Ability System Component for combat: installs the §41 modifier policy and
	 * connects its absorption and status components. Call once per unit.
	 */
	VEYRACOMBAT_API void ConfigureCombatant(UAbilitySystemComponent& AbilitySystem, UVeyraDamageAbsorptionComponent& Absorption,
		UVeyraStatusComponent& Statuses);

	/** Sets a unit's base Max Health from its data and fills its Health. Returns false if refused. */
	VEYRACOMBAT_API bool InitializeVitals(UAbilitySystemComponent& AbilitySystem, double MaxHealth);

	/** Sets a unit's base Move Speed from its data. Returns false if refused. */
	VEYRACOMBAT_API bool InitializeMoveSpeed(UAbilitySystemComponent& AbilitySystem, double MoveSpeed);

	/**
	 * Sets a unit's base Max Resource from its data and fills its Resource (Combat Bible §27). 0 means
	 * the unit has no resource. Returns false if refused.
	 */
	VEYRACOMBAT_API bool InitializeResource(UAbilitySystemComponent& AbilitySystem, double MaxResource);

	/**
	 * Sets every base stat from a unit's data and fills its Health and Resource (ADR-008 §2). Max
	 * Health, Move Speed and Attack Speed must be above 0; the rest at least 0; all finite. Returns
	 * false, changing nothing, if refused.
	 */
	VEYRACOMBAT_API bool InitializeStats(UAbilitySystemComponent& AbilitySystem, const FVeyraStatBlock& Stats);

	/**
	 * Raises the base stats by Growth when a unit levels up (Economy & Progression Bible §9). Health
	 * and Resource rise by the same flat amount as their maximums, so the bars do not refill: the
	 * amount missing before the level-up is still missing after it. That supersedes the Combat Bible
	 * §41 rule of keeping the percentage. Every value must be finite and at least 0. Returns false,
	 * changing nothing, if refused.
	 */
	VEYRACOMBAT_API bool GrowBaseStats(UAbilitySystemComponent& AbilitySystem, const FVeyraStatBlock& Growth);

	/**
	 * Restores Amount of the unit's resource, never above its maximum (Combat Bible §27). Returns false
	 * if refused: Amount must be finite and at least 0.
	 */
	VEYRACOMBAT_API bool RestoreResource(UAbilitySystemComponent& AbilitySystem, double Amount);

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
	 * Prepares one damage event from Source (Combat Bible §50): the source's offence, its Damage
	 * Amplification and penetration, is fixed now, while each target's defences are read when it is
	 * dealt. Each damage type may appear once, with a finite amount of at least 0; the event's own
	 * penetration must have Flat at least 0 and Retained within [0, 1]. Returns an invalid preparation
	 * if refused.
	 */
	VEYRACOMBAT_API FVeyraPreparedDamage PrepareDamage(UAbilitySystemComponent& Source, const FVeyraRawDamageEvent& Damage);

	/**
	 * Deals prepared damage to Target through the canonical pipeline (Combat Bible §25). One
	 * preparation can be dealt to several targets. A target whose death is final takes no damage.
	 * Returns false if refused.
	 */
	VEYRACOMBAT_API bool DealPreparedDamage(const FVeyraPreparedDamage& Damage, UAbilitySystemComponent& Target);

	/** Prepares one damage event from Source and deals it to Target at once. Returns false if refused. */
	VEYRACOMBAT_API bool DealDamage(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target, const FVeyraRawDamageEvent& Damage);

	/**
	 * Grants Target a shield from Source (Combat Bible §7; UVeyraDamageAbsorptionComponent::GrantShield).
	 * Returns its effect, or an invalid handle if refused or there is no room for it.
	 */
	VEYRACOMBAT_API FActiveGameplayEffectHandle GrantShield(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target, const FVeyraShieldGrant& Grant);

	/** Grants Target a shield with no identity, which never merges with another. */
	VEYRACOMBAT_API FActiveGameplayEffectHandle GrantShield(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target,
		EVeyraShieldCategory Category, double Amount, double DurationSeconds);

	/** Grants Target Temporary Health (Combat Bible §7). Returns its effect, or an invalid handle if refused. */
	VEYRACOMBAT_API FActiveGameplayEffectHandle GrantTemporaryHealth(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target,
		double Amount, double DurationSeconds);

	/**
	 * Applies Status from Source to Target under its stacking policy (Combat Bible §8, §46;
	 * UVeyraStatusComponent::Apply). A target whose death is final, or that has no status ledger,
	 * refuses it. Returns false if refused.
	 */
	VEYRACOMBAT_API bool ApplyStatus(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target, const FVeyraStatusSpec& Status);

	/** Ends Target's status Id early, from every source, as when a recast ends a buff. Returns whether it had one. */
	VEYRACOMBAT_API bool RemoveStatus(UAbilitySystemComponent& Target, const FVeyraContentId& Id);

	/** The actions Unit's statuses stop it taking now (Combat Bible §8). None when it has no status ledger. */
	VEYRACOMBAT_API EVeyraActionBlocks GetActionBlocks(const UAbilitySystemComponent& Unit);
}
