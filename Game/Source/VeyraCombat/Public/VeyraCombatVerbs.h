// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Absorption/VeyraAbsorptionLedger.h"
#include "ActiveGameplayEffectHandle.h"
#include "Damage/VeyraDamageTypes.h"
#include "GameplayEffectTypes.h"
#include "Movement/VeyraForcedMovementTypes.h"
#include "Stats/VeyraEquipmentStats.h"
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

	/** How it is delivered, which decides whether it can damage a structure (Combat Bible §33). */
	EVeyraDamageDelivery Delivery = EVeyraDamageDelivery::Ability;

	/** Whether it is a basic attack that crit (Combat Bible §5), which its dealt-damage event carries (ADR-025 §6). */
	bool bCritical = false;

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

	/**
	 * Sets a unit's base Armor and Magic Resist from its data, as a structure's own defences
	 * (Combat Bible §33). Both must be finite and at least 0. Returns false if refused.
	 */
	VEYRACOMBAT_API bool InitializeResistances(UAbilitySystemComponent& AbilitySystem, double Armor, double MagicResist);

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
	 * Scales a unit that grows stronger from outside its own stats, as Team Flux strengthens Fluxborn
	 * (Battleground Bible §4; ADR-011 §10): its base Max Health becomes BaseMaxHealth times
	 * HealthMultiplier, with Health keeping its percentage (Combat Bible §41), and its base outgoing
	 * damage becomes DamageMultiplier. Each call replaces the last. Values must be finite and above 0.
	 * Returns false, changing nothing, if refused.
	 */
	VEYRACOMBAT_API bool SetUnitScaling(UAbilitySystemComponent& AbilitySystem, double BaseMaxHealth, double HealthMultiplier, double DamageMultiplier);

	/**
	 * Sets the damage reduction a unit carries of its own, outside any status, as a structure's backdoor
	 * protection (Combat Bible §33): its base incoming damage becomes 1 − Fraction. True Damage skips it
	 * (§25). Fraction must be finite, at least 0 and below 1. Returns false, changing nothing, if refused.
	 */
	VEYRACOMBAT_API bool SetBaseDamageReduction(UAbilitySystemComponent& AbilitySystem, double Fraction);

	/**
	 * Sets what a unit's equipment adds to its stats (ADR-012 §6), replacing whatever it added before,
	 * so a change of equipment needs no bookkeeping. Health keeps its percentage of Max Health (Combat
	 * Bible §41); a dead unit stays at 0. All-zero stats remove the equipment's effect. Every value must
	 * be finite and at least 0. Returns false, changing nothing, if refused.
	 */
	VEYRACOMBAT_API bool SetEquipmentStats(UAbilitySystemComponent& AbilitySystem, const FVeyraEquipmentStats& Stats);

	/**
	 * Restores Amount of the unit's resource, never above its maximum (Combat Bible §27). Returns false
	 * if refused: Amount must be finite and at least 0.
	 */
	VEYRACOMBAT_API bool RestoreResource(UAbilitySystemComponent& AbilitySystem, double Amount);

	/**
	 * Restores Amount of the unit's Health, never above its Max Health (Combat Bible §6: restoration
	 * never overheals). Health Regeneration, the Prime Well's regeneration and fountain recovery use
	 * it (ADR-011 §9, §11). A unit whose death is final restores nothing. Returns false if refused:
	 * Amount must be finite and at least 0, and the unit needs a UVeyraVitalsSet.
	 */
	VEYRACOMBAT_API bool RestoreHealth(UAbilitySystemComponent& AbilitySystem, double Amount);

	/**
	 * Server: restores Amount of Health to Target as a heal Provider gives, as an ability or an item
	 * does, never above Max Health (Combat Bible §6). Returns the Health actually restored, which
	 * statistics credit to Provider (ADR-017 §1); 0 for a dead unit or a refused amount.
	 */
	VEYRACOMBAT_API double RestoreHealthFrom(UAbilitySystemComponent& Provider, UAbilitySystemComponent& Target, double Amount);

	/**
	 * Makes the unit invulnerable (Combat Bible §10) until a matching RevokeInvulnerability. Grants
	 * count, so each grant needs its own revoke; statuses that make a unit invulnerable count
	 * separately. Structures use it for their prerequisites (Battleground Bible §18). Server only.
	 */
	VEYRACOMBAT_API void GrantInvulnerability(UAbilitySystemComponent& AbilitySystem);

	/** Ends one GrantInvulnerability. */
	VEYRACOMBAT_API void RevokeInvulnerability(UAbilitySystemComponent& AbilitySystem);

	/** Whether the unit is invulnerable now, by a grant or a status. */
	VEYRACOMBAT_API bool IsInvulnerable(const UAbilitySystemComponent& AbilitySystem);

	/** Whether the unit has at least Amount of its resource. A cost of 0 is always affordable. */
	VEYRACOMBAT_API bool CanAffordResource(const UAbilitySystemComponent& AbilitySystem, double Amount);

	/**
	 * Pays Amount of the unit's resource (Combat Bible §27). Refused, changing nothing, if the unit
	 * cannot afford it: resources never go negative. Returns false if refused.
	 */
	VEYRACOMBAT_API bool SpendResource(UAbilitySystemComponent& AbilitySystem, double Amount);

	/**
	 * Brings a dead unit back for its respawn: alive, with full Health and Resource (Combat Bible
	 * §18), or the resource it kept (ADR-033 §1). Returns false if the unit was not dead.
	 */
	VEYRACOMBAT_API bool Revive(UAbilitySystemComponent& AbilitySystem);

	/**
	 * Server: the unit's resource empties and is kept from then on, as Charge (ADR-033 §1): initializing
	 * its stats, reviving it and the fountain leave it be, and only effects restore it. False for a unit
	 * with no resource.
	 */
	VEYRACOMBAT_API bool KeepResource(UAbilitySystemComponent& AbilitySystem);

	/** Whether the unit keeps its resource rather than having it refilled (ADR-033 §1). */
	VEYRACOMBAT_API bool IsResourceKept(const UAbilitySystemComponent& AbilitySystem);

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
	 * preparation can be dealt to several targets. A target whose death is final takes no damage, and
	 * a structure takes none unless its delivery can damage structures (§33). Returns false if refused.
	 */
	VEYRACOMBAT_API bool DealPreparedDamage(const FVeyraPreparedDamage& Damage, UAbilitySystemComponent& Target);

	/**
	 * Deals prepared damage to Target with AddedAtImpact joining the same event, each amount added to
	 * its type's component: values the target decides when the damage lands, such as a bonus for its
	 * missing Health (Combat Bible §50). It stays one hit for mitigation and shields (§25). Each added
	 * amount must be finite and at least 0. Returns false if refused.
	 */
	VEYRACOMBAT_API bool DealPreparedDamage(const FVeyraPreparedDamage& Damage, UAbilitySystemComponent& Target, TConstArrayView<FVeyraDamageComponent> AddedAtImpact);

	/** The Health a unit lacks: its Max Health less its Health, never below 0. */
	VEYRACOMBAT_API double GetMissingHealth(const UAbilitySystemComponent& Unit);

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
	 * Grants Target Temporary Health named Id from Source (Combat Bible §7): the same name from the same
	 * source is one grant, topped up by Amount to at most MaxAmount, its duration started again
	 * (UVeyraDamageAbsorptionComponent::GrantTemporaryHealth). Invalid if refused or already at its most.
	 */
	VEYRACOMBAT_API FActiveGameplayEffectHandle GrantTemporaryHealth(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target,
		const FVeyraContentId& Id, double Amount, double MaxAmount, double DurationSeconds);

	/**
	 * Applies Status from Source to Target under its stacking policy (Combat Bible §8, §46;
	 * UVeyraStatusComponent::Apply). A target whose death is final, or that has no status ledger,
	 * refuses it, and a structure refuses statuses from its enemies (§33: crowd control and debuffs
	 * do not affect structures). Returns false if refused.
	 */
	VEYRACOMBAT_API bool ApplyStatus(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target, const FVeyraStatusSpec& Status);

	/** Ends Target's status Id early, from every source, as when a recast ends a buff. Returns whether it had one. */
	VEYRACOMBAT_API bool RemoveStatus(UAbilitySystemComponent& Target, const FVeyraContentId& Id);

	/**
	 * Whether a Spell Shield Target holds blocks a hostile ability hit from Source (Combat Bible §19;
	 * ADR-025 §4). If so, the shield is consumed and announced, and the hit must deal no damage, apply
	 * no status or displacement, and count as no hit. Every site that lands a discrete hostile ability
	 * hit asks first: effect delivery, targeted damage and tethers. Basic attacks, Procs and effects
	 * that apply over time do not. False for an ally's hit, and on a machine without authority.
	 */
	VEYRACOMBAT_API bool BlockAbilityHit(UAbilitySystemComponent& Target, UAbilitySystemComponent& Source);

	/**
	 * Server: ends every Camouflage and Invisibility on Unit, which attacked or cast something offensive
	 * (Combat Bible §11; ADR-018 §4; ADR-030 §1). Damage taken does not end them.
	 */
	VEYRACOMBAT_API void EndStealth(UAbilitySystemComponent& Unit);

	/** Whether Unit, a body or a participant, holds the status Id that Source applied (ADR-030 §7): a caster's mark. */
	VEYRACOMBAT_API bool HasStatusFrom(const AActor* Unit, const FVeyraContentId& Id, const UAbilitySystemComponent& Source);

	/** The actions Unit's statuses stop it taking now (Combat Bible §8). None when it has no status ledger. */
	VEYRACOMBAT_API EVeyraActionBlocks GetActionBlocks(const UAbilitySystemComponent& Unit);

	/**
	 * Server only: Unit's body holds on to Host's for Seconds and goes where it goes (ADR-018 §2).
	 * Meanwhile it cannot attack, and may cast (ADR-018 §8). False if refused: a dead unit or host,
	 * a unit without a body, or one a displacement or its statuses hold.
	 */
	VEYRACOMBAT_API bool Attach(UAbilitySystemComponent& Unit, AActor& Host, double Seconds);

	/** Server only: Unit lets go of its host, if it holds one. */
	VEYRACOMBAT_API void Detach(UAbilitySystemComponent& Unit);

	/** The body Unit holds on to now; nullptr if none. */
	VEYRACOMBAT_API AActor* GetAttachHost(const UAbilitySystemComponent& Unit);

	/**
	 * Server only: Unit rides (Combat Bible §56): its speed is set, its turns limited, it passes through
	 * units, and it cannot attack meanwhile. False for a dead unit, one without a body, or invalid values.
	 */
	VEYRACOMBAT_API bool StartRide(UAbilitySystemComponent& Unit, const FVeyraRide& Ride);

	/** Server only: Unit's ride ends, for Reason, if it rides. */
	VEYRACOMBAT_API void EndRide(UAbilitySystemComponent& Unit, EVeyraRideEndReason Reason);

	/** Whether Unit rides now. */
	VEYRACOMBAT_API bool IsRiding(const UAbilitySystemComponent& Unit);

	/**
	 * Displaces Target's body, a Knockback or a Pull from Source (Combat Bible §8, §9; ADR-009 §2).
	 * Displacement Resistance shortens it; it interrupts the target and replaces an older displacement
	 * or a dash. The displacement lands even when terrain leaves no room to move. A target whose death
	 * is final, or that has no body that can be displaced, refuses it. Returns false if refused.
	 */
	VEYRACOMBAT_API bool Displace(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target, const FVeyraDisplacement& Displacement);

	/**
	 * Dashes Unit's body (Combat Bible §9). Refused, returning false, while the unit is dead, stunned,
	 * rooted, grounded, displaced or already dashing, or for values out of range.
	 */
	VEYRACOMBAT_API bool Dash(UAbilitySystemComponent& Unit, const FVeyraDash& Dash);

	/**
	 * Blinks Unit's body to the nearest legal ground at Destination, facing Facing unless it is zero
	 * (Combat Bible §9; ADR-030 §4): an instant move with no path, which terrain between does not stop.
	 * It ends a dash under way and the body's move. Refused, returning false, while the unit is dead,
	 * rooted, grounded, stunned, displaced or held on, or where no legal ground is near.
	 */
	VEYRACOMBAT_API bool Blink(UAbilitySystemComponent& Unit, const FVector& Destination, const FVector& Facing = FVector::ZeroVector);

	/**
	 * Blinks Unit's body beside Target, Distance from its edge on Unit's side of it, facing it (ADR-030
	 * §9; ADR-031 §5). OutLanding and OutFacing say where it landed and which way it faces. False if it
	 * could not blink.
	 */
	VEYRACOMBAT_API bool BlinkBeside(UAbilitySystemComponent& Unit, const AActor& Target, double Distance, FVector& OutLanding, FVector& OutFacing);

	/**
	 * The navigable ground nearest Point within Combat's reach for forced movement, in X and Y, at Point's
	 * height; Point itself where there is none (ADR-031 §4).
	 */
	VEYRACOMBAT_API FVector NearestGround(const UWorld& World, const FVector& Point);

	/**
	 * Holds Unit's body in place for its own cast, or lets it go (Combat Bible §48). Its orders wait
	 * meanwhile. Does nothing for a unit with no body.
	 */
	VEYRACOMBAT_API void SetCastLocksMovement(UAbilitySystemComponent& Unit, bool bLocks);
}
