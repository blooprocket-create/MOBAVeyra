// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Damage/VeyraDamageTypes.h"
#include "Misc/EnumClassFlags.h"
#include "Units/VeyraUnit.h"
#include "UObject/ObjectMacros.h"

#include "VeyraStatusTypes.generated.h"

/**
 * What a status does (ADR-009 §1). Each kind reads its Magnitude in one way, given beside it. The
 * stat kinds change their attribute through one percentage modifier (Combat Bible §41).
 */
UENUM()
enum class EVeyraStatusKind : uint8
{
	/** Cannot move, basic attack or cast (Combat Bible §8). Crowd control. Magnitude: 0. */
	Stun,
	/** Reduces Movement Speed; the strongest Slow controls it (§8, §23). Crowd control. Magnitude: the fraction removed, above 0 and below 1. */
	Slow,
	/** Changes Move Speed. Not crowd control, even when negative. Magnitude: the change per stack, above -1 and not 0; 0.2 is 20% faster. */
	MoveSpeed,
	/** Changes Attack Speed. Magnitude: the change per stack, above -1 and not 0. */
	AttackSpeed,
	/** Shortens crowd control that lands later (§8). Magnitude: the fraction removed per stack, above 0 and below 1. */
	Tenacity,
	/** Reduces damage taken (§15). Magnitude: the fraction removed per stack, above 0 and below 1. */
	DamageReduction,
	/** Shortens forced displacement (§9). Magnitude: the fraction removed per stack, above 0 and below 1. */
	DisplacementResistance,
	/**
	 * Basic attacks also hit the other enemies in the attacker's cleave area for part of their damage
	 * (ADR-009 §5); the strongest applies. Not crowd control. Magnitude: that fraction, above 0 and at most 1.
	 */
	AttackCleave,
	/**
	 * Movement Speed while the unit moves toward an enemy Vanguard (Combat Bible §23, a conditional
	 * bonus; ADR-008 §9), the strongest applying. Not crowd control. Magnitude: the fraction added,
	 * above 0 and at most 1.
	 */
	MoveSpeedTowardEnemyVanguards,
	/** Changes Health Regeneration. Not crowd control. Magnitude: the change per stack, above -1 and not 0; 1 doubles it. */
	HealthRegeneration,
	/**
	 * Increases the damage the unit deals, the counterpart of DamageReduction. Not crowd control.
	 * Magnitude: the fraction added per stack, above 0, every stack together at most 1.
	 */
	DamageAmplification,
	/**
	 * Deals damage of the status's DamageType in its source's name every TickSeconds while it lasts
	 * (Combat Bible §14): no tick as it lands and no partial tick as it ends. Not crowd control.
	 * Magnitude: the damage of one tick per stack, above 0.
	 */
	DamageOverTime,
	/**
	 * Reduces the damage the unit deals, the hostile counterpart of DamageAmplification (ADR-015 §3).
	 * Like generic reduction it spares True damage (§15). Not crowd control. Magnitude: the fraction
	 * removed per stack, above 0, every stack together below 1.
	 */
	Weaken,
	/**
	 * Adds to the unit's basic-attack range (ADR-018 §2). Not crowd control. Magnitude: the distance
	 * added per stack, above 0; every entry adds.
	 */
	AttackRange,
	/**
	 * Raises the unit's Attack Speed cap for a while (Combat Bible §22); overflow is still measured from
	 * the ordinary cap. Not crowd control. Magnitude: the raised cap in attacks per second, above 0; the
	 * strongest applies, and a cap below the ordinary one changes nothing.
	 */
	AttackSpeedCap,
	/**
	 * Weakens the Slows on the unit (ADR-018 §2). Not crowd control. Magnitude: the fraction of each
	 * Slow removed per stack, above 0, every stack together below 1; entries multiply, as Tenacity does.
	 */
	SlowResistance,
	/**
	 * The unit has chosen to stand still, such as in a firing stance (ADR-018 §2): it cannot move, and
	 * may attack and cast. Not crowd control: Tenacity does not shorten it. Magnitude: 0.
	 */
	Planted,
	/**
	 * Hidden from enemies beyond a detection radius (Combat Bible §11; ADR-018 §4). Not crowd control;
	 * attacking or an offensive cast ends it. Magnitude: the detection radius, above 0.
	 */
	Camouflage,
	/**
	 * The status's source reaches further with its basic attacks against this unit (ADR-018 §2), such
	 * as Kade against a Tracked target. Not crowd control. Magnitude: the distance added per stack, above 0.
	 */
	SourceAttackRange,
	/**
	 * A mark or meter with no effect of its own, which a passive reads (ADR-018 §2): Mimzi's Hex, and
	 * later Raska's Momentum. Not crowd control. Magnitude: 0.
	 */
	Counter,
	/**
	 * The unit's own choice to take no action, as Patch's Play Dead (ADR-018 §2): it cannot move,
	 * attack or cast. Not crowd control; Tenacity and Cleanse ignore it. Magnitude: 0.
	 */
	Dormant,
	/** Ordinary crowd control and displacement from enemies cannot affect the unit (Combat Bible §8, §9). Magnitude: 0. */
	Unstoppable,
	/** No displacement or Knockup affects the unit (Combat Bible §9). Magnitude: 0. */
	DisplacementImmunity,
	/**
	 * Crowd control: the unit walks away from the status's source and cannot attack or cast (Combat
	 * Bible §8). Magnitude: the Slow it walks under, in [0, 1).
	 */
	Fear,
	/** Crowd control: airborne, taking no action (Combat Bible §8, §9). Tenacity does not shorten it. Magnitude: 0. */
	Knockup,
	/** The unit passes through other units, never terrain (Combat Bible §24). Magnitude: 0. */
	Ghosted,
	/** The unit's body is this many times as wide (Combat Bible §13). Magnitude: the scale, above 0; one stack. */
	BodyScale,
	/**
	 * Damage from a source within ArcDegrees of the unit's facing is reduced (ADR-018 §2), as Raska's
	 * Countersteer. Magnitude: the fraction removed per stack, below 1 in all.
	 */
	DirectionalDamageReduction,
	/**
	 * The unit's basic attacks deal more (ADR-018 §2), against UnitKinds only when it names any.
	 * Magnitude: the fraction added per stack, above 0.
	 */
	AttackDamageAmplification,
	/**
	 * Reduces the unit's Magic Resistance by a percentage (Combat Bible §3; ADR-023 §5), as Fracture
	 * does; entries multiply, as other percentage reductions do, and flat reduction comes after. Not
	 * crowd control. Magnitude: the fraction removed per stack, above 0, every stack together below 1.
	 */
	MagicResistReduction,
	/**
	 * A Spell Shield (Combat Bible §19; ADR-025 §4): the next hostile ability hit on the unit is blocked
	 * whole and consumes it (VeyraCombat::BlockAbilityHit). Basic attacks, Procs and effects over time
	 * pass. Not crowd control. Magnitude: 0.
	 */
	SpellShield,
	/**
	 * Rooted (Combat Bible §8; ADR-026 §3): the unit cannot move, nor cast an ability that moves it, and
	 * may attack and cast the rest. Crowd control: Tenacity shortens it. Magnitude: 0.
	 */
	Root,
	/**
	 * The unit may walk while its basic attack winds up, keeping this share of its Movement Speed, and a
	 * move order does not cancel the windup (ADR-027 §1); the strongest applies. Not crowd control.
	 * Magnitude: the share, above 0 and at most 1; one stack.
	 */
	MobileAttack,
	/**
	 * Blinded (ADR-028 §1): the unit's basic attacks miss. Each still counts as an attack, spending its
	 * time and any empowerment, but lands nothing: no damage, no on-hit, no secondary impact. Crowd
	 * control: Tenacity shortens it. Magnitude: 0.
	 */
	Blind,
	/**
	 * Grounded (ADR-028 §2): the unit cannot cast an ability that moves it (a dash, a leap or an attach),
	 * and may walk, attack and cast the rest; a dash under way finishes. Crowd control: Tenacity shortens
	 * it. Magnitude: 0.
	 */
	Grounded,
	/**
	 * Invisible (Combat Bible §11; ADR-030 §1): hidden from enemies at any distance, and only True Sight
	 * reveals it. Not crowd control; attacking or an offensive cast ends it, as it ends Camouflage.
	 * Magnitude: 0.
	 */
	Invisible,
	/**
	 * Untargetable (Combat Bible §10; ADR-030 §2): enemies cannot acquire it, their skillshots, areas and
	 * cleaves pass over it, and a targeted projectile flying at it fails on arrival. Not crowd control; it
	 * cleanses nothing. Magnitude: 0.
	 */
	Untargetable,
	/**
	 * ResourceCostReduction (ADR-033 §3): the unit's abilities cost less, each reduction leaving its share
	 * of the cost. Magnitude: the share taken off, above 0 and below 1 with every stack.
	 */
	ResourceCostReduction,
	/**
	 * AttackShortensCooldown (ADR-033 §6): each of its holder's committed basic attacks shortens the remaining
	 * cooldown of whichever of its Q, W and E comes off cooldown soonest. Magnitude: the seconds, above 0.
	 */
	AttackShortensCooldown,
	/**
	 * MaxHealth (ADR-034 §2): the unit's Max Health is multiplied by one plus its magnitude while it holds,
	 * Health keeping its share of the maximum, as a companion's true form. Magnitude: a signed change per
	 * stack, every stack together above -1.
	 */
	MaxHealth,
	/**
	 * Sounded (ADR-036 §2): while its holder stands in Dense Fog, the fog pings its presence to its source's
	 * side, at the presence cadence. It reveals nothing, outlines nothing and grants no targeting; outside
	 * fog it does nothing. Not crowd control. Magnitude: 0.
	 */
	Sounded,
	/**
	 * Cover (Combat Bible §20; ADR-037 §4): its holder shelters its allies behind it, against its facing and within
	 * its reach, from projectiles whose source lies in front of it within its arc. It prevents a share of their
	 * damage out of its capacity, and passes some of what it prevents to its holder. Not crowd control.
	 * Magnitude: the share prevented, above 0 and below 1; one stack.
	 */
	Cover,
	/**
	 * Designated (ADR-037 §5): its source's companion prefers it among the targets it may attack anyway. It
	 * reveals nothing and extends no range. Not crowd control. Magnitude: 0.
	 */
	Designated,
	/**
	 * Stasis (Combat Bible §10; ADR-050 §1): the unit cannot move, attack or cast, item Actives and Flux Spells
	 * included; it is Untargetable and takes no damage, True Damage included, so effects over time tick for 0. No
	 * enemy status, no displacement, no heal and no shield lands on it; its Health Regeneration continues. Not crowd
	 * control: Tenacity does not shorten it, and only the effect that applied it ends it early. Magnitude: 0.
	 */
	Stasis,
};

/** How a new application meets an active status with the same ID (Combat Bible §46). */
UENUM()
enum class EVeyraStackingPolicy : uint8
{
	/** One instance on the unit; a new application replaces it and restarts its duration. */
	UniqueRefresh,
	/** One instance on the unit; a new application replaces it only if stronger, or equally strong and lasting longer. */
	UniqueReplaceStrongest,
	/** One instance on the unit; each application adds a stack, up to MaxStacks, and restarts the duration. */
	Stacking,
	/** One instance per source; each source's refreshes as UniqueRefresh. */
	IndependentSources,
};

/** The actions a unit's statuses stop it taking (Combat Bible §8). */
enum class EVeyraActionBlocks : uint8
{
	None = 0,
	Move = 1 << 0,
	Attack = 1 << 1,
	Cast = 1 << 2,
	/** Abilities that move their caster: dashes, leaps and attaching (ADR-026 §3). */
	Dash = 1 << 3,
};
ENUM_CLASS_FLAGS(EVeyraActionBlocks);

/**
 * One status an effect applies, as its data declares it. Abilities' tuning binds these records;
 * Combat checks them (VeyraStatuses::Validate) before applying one.
 */
USTRUCT()
struct VEYRACOMBAT_API FVeyraStatusSpec
{
	GENERATED_BODY()

	/** The status's stable identity (Combat Bible §46). The same ID is the same status for stacking. */
	UPROPERTY()
	FVeyraContentId Id;

	UPROPERTY()
	EVeyraStatusKind Kind = EVeyraStatusKind::Stun;

	UPROPERTY()
	EVeyraStackingPolicy Stacking = EVeyraStackingPolicy::UniqueRefresh;

	/** Read as its kind declares. */
	UPROPERTY()
	double Magnitude = 0.0;

	/** Before Tenacity, which shortens only crowd control. */
	UPROPERTY()
	double DurationSeconds = 0.0;

	/** The most stacks the status can hold: at least 1, and exactly 1 unless it stacks. */
	UPROPERTY()
	int32 MaxStacks = 1;

	/**
	 * Seconds each takedown by the unit adds to the status's remaining time (ADR-009 §1), up to
	 * TakedownExtensionMaxSeconds in all. Both 0 for a status takedowns do not extend.
	 */
	UPROPERTY()
	double TakedownExtensionSeconds = 0.0;

	UPROPERTY()
	double TakedownExtensionMaxSeconds = 0.0;

	/** A DamageOverTime status's damage type; unused by other kinds. */
	UPROPERTY()
	EVeyraDamageType DamageType = EVeyraDamageType::Physical;

	/** Seconds between a DamageOverTime status's ticks, above 0 and at most its duration; 0 for every other kind. */
	UPROPERTY()
	double TickSeconds = 0.0;

	/**
	 * For a stacking status: when its duration runs out it loses one stack, not all, and runs this long
	 * again, until the last stack goes (ADR-018 §2). 0 to end at once, as every other status does.
	 */
	UPROPERTY()
	double StackDecaySeconds = 0.0;

	/** DirectionalDamageReduction and Cover: the arc of the unit's facing it guards, in degrees; 0 for any other kind. */
	UPROPERTY()
	double ArcDegrees = 0.0;

	/** AttackDamageAmplification: the unit kinds it amplifies attacks against; Cover: the kinds of ally it shelters. Empty for all, and for any other kind. */
	UPROPERTY()
	TArray<EVeyraUnitKind> UnitKinds;

	/** The kinds of unit it lands on, as Korruk's Splinters embed only in Vanguards (ADR-026 §2); empty for every kind. */
	UPROPERTY()
	TArray<EVeyraUnitKind> LandsOn;

	/** How many of its holder's basic attacks it lasts, each that commits spending one (ADR-033 §4); 0 for a status attacks do not spend. */
	UPROPERTY()
	int32 AttackCharges = 0;

	/** Cover: how far behind its holder it shelters, how much it prevents in all, and the share of that its holder takes; 0 for any other kind. */
	UPROPERTY()
	double CoverReach = 0.0;

	UPROPERTY()
	double CoverCapacity = 0.0;

	UPROPERTY()
	double CoverTransferShare = 0.0;
};

/** One active status as every machine sees it. Replicated for presentation. */
USTRUCT()
struct FVeyraStatusEntry
{
	GENERATED_BODY()

	/** Identifies the entry for as long as it lasts, through refreshes and new stacks. */
	UPROPERTY()
	int32 Sequence = 0;

	UPROPERTY()
	FVeyraContentId Id;

	UPROPERTY()
	EVeyraStatusKind Kind = EVeyraStatusKind::Stun;

	/** Per stack. */
	UPROPERTY()
	double Magnitude = 0.0;

	UPROPERTY()
	int32 Stacks = 1;

	/** When the current application began and when it ends, in the server's world time. */
	UPROPERTY()
	double StartedAt = 0.0;

	UPROPERTY()
	double EndsAt = 0.0;

	/** The basic attacks it has left before it ends (ADR-033 §4); 0 for one attacks do not spend. */
	UPROPERTY()
	int32 AttackCharges = 0;
};

/** A unit's active statuses. */
USTRUCT()
struct FVeyraStatusLedger
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FVeyraStatusEntry> Entries;
};

/** Combat's status rules (Combat Bible §8, §46), as plain functions. */
namespace VeyraStatuses
{
	/** Problems with Spec, each a field name and a message; empty when it can be applied. */
	VEYRACOMBAT_API TArray<FString> Validate(const FVeyraStatusSpec& Spec);

	/** Whether Tenacity shortens the kind: crowd control does, buffs and speed changes do not (§8). */
	VEYRACOMBAT_API bool IsTenacityReducible(EVeyraStatusKind Kind);

	/** Whether the kind is ordinary crowd control, which Unstoppable refuses (§8): Stun, Slow, Fear and Knockup. */
	VEYRACOMBAT_API bool IsCrowdControl(EVeyraStatusKind Kind);

	/**
	 * Whether a status of Kind and Magnitude harms its bearer (ADR-059 §4): crowd control, damage over time, Weaken, Magic
	 * Resist reduction, a source's reach or companion turned on it, Sounded, and a signed change below 0. A unit's own
	 * choices (Planted, Dormant, Stasis) and a mark with no effect of its own do not. It describes a status; it changes none.
	 */
	VEYRACOMBAT_API bool IsHarmful(EVeyraStatusKind Kind, double Magnitude);

	/**
	 * A reducible duration after Tenacity: DurationSeconds times TenacityRetained, but never below
	 * FloorSeconds, and never longer than it started (§8).
	 */
	VEYRACOMBAT_API double ApplyTenacity(double DurationSeconds, double TenacityRetained, double FloorSeconds);

	/**
	 * The multiplier a stat kind's modifier applies for Stacks stacks: 1 + Magnitude × Stacks for a
	 * change, 1 - Magnitude × Stacks for a reduction. Unused for Stun and Slow.
	 */
	VEYRACOMBAT_API double StatMultiplier(EVeyraStatusKind Kind, double Magnitude, int32 Stacks);

	/** Whether a new application replaces an active one under UniqueReplaceStrongest. */
	VEYRACOMBAT_API bool IsStronger(const FVeyraStatusEntry& Active, double Magnitude, double EndsAt);

	/** The largest magnitude among the entries of Kind; 0 when there is none. */
	VEYRACOMBAT_API double Strongest(TConstArrayView<FVeyraStatusEntry> Entries, EVeyraStatusKind Kind);

	/** The entries of Kind added together, each its magnitude times its stacks; 0 when there is none. */
	VEYRACOMBAT_API double Total(TConstArrayView<FVeyraStatusEntry> Entries, EVeyraStatusKind Kind);

	/**
	 * What a reduction kind leaves: the product of 1 - magnitude × stacks over the entries of Kind, as
	 * Tenacity combines (§8); 1 when there is none.
	 */
	VEYRACOMBAT_API double Retained(TConstArrayView<FVeyraStatusEntry> Entries, EVeyraStatusKind Kind);

	/** The fraction of speed the strongest Slow removes; 0 when there is none (§8). */
	VEYRACOMBAT_API double StrongestSlow(TConstArrayView<FVeyraStatusEntry> Entries);

	/** The actions the entries block. */
	VEYRACOMBAT_API EVeyraActionBlocks ActionBlocks(TConstArrayView<FVeyraStatusEntry> Entries);

	/** How many whole ticks a DamageOverTime status of DurationSeconds deals, one every TickSeconds (§14). */
	VEYRACOMBAT_API int32 TickCount(double DurationSeconds, double TickSeconds);

	/**
	 * Whether a cover held at HolderAt, facing Facing, shelters a unit at ShelteredAt from a projectile whose source
	 * stands at SourceAt (ADR-037 §4): the unit is behind the cover or beside it, against its facing, within Reach of
	 * it, and the source lies within ArcDegrees of its facing. Judged on the ground plane.
	 */
	VEYRACOMBAT_API bool Shelters(const FVector& HolderAt, const FVector& Facing, double ArcDegrees, double Reach, const FVector& ShelteredAt, const FVector& SourceAt);
}
