// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Absorption/VeyraAbsorptionLedger.h"
#include "Slots/VeyraAbilitySlot.h"
#include "Content/VeyraContentId.h"
#include "Damage/VeyraDamageTypes.h"
#include "Movement/VeyraForcedMovementTypes.h"
#include "Shapes/VeyraShapes.h"
#include "Statuses/VeyraStatusTypes.h"
#include "Tuning/VeyraTuningProvenance.h"
#include "UObject/ObjectMacros.h"
#include "Units/VeyraUnit.h"

#include "VeyraAbilitiesTuning.generated.h"

// The Abilities domain's tuning, bound from Game/Tuning/Abilities.json (ADR-006 §6, ADR-008 §3). The
// schema holds every range; a 0 here only means "not loaded". A field whose name ends "ByRank" holds
// one value for every rank, or one per rank (VeyraAbilityRules::ValueAtRank).

/**
 * One targeted, instant ability that deals one damage component, and may put statuses on its target
 * (Combat Bible §29; ADR-015 §3).
 */
USTRUCT()
struct FVeyraTargetedDamageAbilityTuning
{
	GENERATED_BODY()

	UPROPERTY()
	double CastRange = 0.0;

	UPROPERTY()
	double CooldownSeconds = 0.0;

	UPROPERTY()
	double ResourceCost = 0.0;

	UPROPERTY()
	EVeyraDamageType DamageType = EVeyraDamageType::Physical;

	/** At the caster's first Level. 0 for an ability that only applies statuses. */
	UPROPERTY()
	double DamageAmount = 0.0;

	/** Added for each Level of the caster's beyond the first, read at Commit. */
	UPROPERTY()
	double DamagePerLevel = 0.0;

	/** Status IDs from the statuses map, put on the target it hits. */
	UPROPERTY()
	TArray<FVeyraContentId> Statuses;

	/** The kinds of unit it may target; empty for any hostile unit. */
	UPROPERTY()
	TArray<EVeyraUnitKind> TargetKinds;
};

/** Whether a caster may move during a phase of its cast (Combat Bible §48). */
UENUM()
enum class EVeyraCastMovement : uint8
{
	Free,
	Locked,
};

/** How a cast is timed and paid for (Combat Bible §26, §27, §48; ADR-008 §4). */
/** What a recast window does if its time runs out unused (ADR-018 §1). */
UENUM()
enum class EVeyraRecastExpiry : uint8
{
	/** The follow-up is lost. */
	Lapse,
	/** The follow-up casts itself, as NO BRAKES' Last Exit does. */
	Cast,
};

/** After a cast commits, its slot holds a follow-up for a while (ADR-018 §1). */
USTRUCT()
struct FVeyraRecastTuning
{
	GENERATED_BODY()

	/** The follow-up the slot holds: another ability's ID. */
	UPROPERTY()
	FVeyraContentId Ability;

	UPROPERTY()
	double WindowSeconds = 0.0;

	UPROPERTY()
	EVeyraRecastExpiry OnExpiry = EVeyraRecastExpiry::Lapse;
};

USTRUCT()
struct FVeyraCastTuning
{
	GENERATED_BODY()

	/** Starts at Commit. */
	UPROPERTY()
	TArray<double> CooldownSecondsByRank;

	/** Paid at Commit. */
	UPROPERTY()
	TArray<double> ResourceCostByRank;

	/** How far from the caster a ground point may be, in units; a point beyond is brought back within it (ADR-008 §9). 0 for an ability that needs no point. */
	UPROPERTY()
	double CastRange = 0.0;

	/** Seconds before Commit. An interruption during them costs nothing and starts part of the cooldown. */
	UPROPERTY()
	double WindupSeconds = 0.0;

	UPROPERTY()
	EVeyraCastMovement WindupMovement = EVeyraCastMovement::Free;

	/** Seconds after delivery before the caster may cast again. */
	UPROPERTY()
	double RecoverySeconds = 0.0;

	/** At most one: the follow-up its slot holds once this cast commits. */
	UPROPERTY()
	TArray<FVeyraRecastTuning> RecastWindow;
};

/** One damage component, from the caster's rank and power at Commit (Combat Bible §25, §50). */
USTRUCT()
struct FVeyraDamageTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraDamageType Type = EVeyraDamageType::Physical;

	UPROPERTY()
	TArray<double> AmountByRank;

	UPROPERTY()
	double PhysicalPowerRatio = 0.0;

	UPROPERTY()
	double MagicPowerRatio = 0.0;
};

/** Which way a displacement moves a unit an area hits. */
UENUM()
enum class EVeyraDisplacementDirection : uint8
{
	/** A Pull toward the area's origin. */
	TowardOrigin,
	/** A Knockback away from the area's origin. */
	AwayFromOrigin,
	/** Sideways to the cast's direction, to the caster's left. */
	AcrossCastLeft,
	/** Sideways to the cast's direction, to the caster's right. */
	AcrossCastRight,
	/** Out of a projectile's path, to the side of it the unit is on (ADR-008 §9: Cairn's hook). */
	AsideFromPath,
};

/** A displacement an effect applies (Combat Bible §9). */
USTRUCT()
struct FVeyraDisplacementTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraDisplacementDirection Direction = EVeyraDisplacementDirection::TowardOrigin;

	/** Units, before Displacement Resistance. A Pull toward the origin stops there. */
	UPROPERTY()
	double Distance = 0.0;

	/** Units per second. */
	UPROPERTY()
	double Speed = 0.0;
};

/**
 * Damage a unit takes for the Health it already lacks, read when the hit lands (Combat Bible §50),
 * such as an artillery shell's bonus against the wounded (Bryn's Last Broadside).
 */
USTRUCT()
struct FVeyraMissingHealthDamageTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraDamageType Type = EVeyraDamageType::Physical;

	/** Of the target's missing Health, added to the hit's component of Type. */
	UPROPERTY()
	double MissingHealthRatio = 0.0;
};

/** What happens to each unit an area hits (ADR-008 §3). */
USTRUCT()
struct FVeyraEffectBundleTuning
{
	GENERATED_BODY()

	/** One component per damage type at most. */
	UPROPERTY()
	TArray<FVeyraDamageTuning> Damage;

	/** Status IDs from the statuses map. */
	UPROPERTY()
	TArray<FVeyraContentId> Statuses;

	/** At most one. */
	UPROPERTY()
	TArray<FVeyraDisplacementTuning> Displacement;

	/** At most one, and only beside Damage: it joins that hit. */
	UPROPERTY()
	TArray<FVeyraMissingHealthDamageTuning> MissingHealthDamage;
};

/** How a DamageOverTime status ticks (Combat Bible §14; ADR-015 §3). */
USTRUCT()
struct FVeyraDamageOverTimeTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraDamageType DamageType = EVeyraDamageType::Physical;

	UPROPERTY()
	double TickSeconds = 0.0;

	/** Added to each tick's damage (the status's magnitude) for each Level of its source's beyond the first, as it lands. */
	UPROPERTY()
	double DamagePerLevel = 0.0;
};

/** One status an ability applies, keyed by its ID (Combat Bible §8, §46; FVeyraStatusSpec). */
USTRUCT()
struct FVeyraStatusTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	UPROPERTY()
	EVeyraStatusKind Kind = EVeyraStatusKind::Stun;

	UPROPERTY()
	EVeyraStackingPolicy Stacking = EVeyraStackingPolicy::UniqueRefresh;

	UPROPERTY()
	double Magnitude = 0.0;

	UPROPERTY()
	double DurationSeconds = 0.0;

	UPROPERTY()
	int32 MaxStacks = 1;

	UPROPERTY()
	double TakedownExtensionSeconds = 0.0;

	UPROPERTY()
	double TakedownExtensionMaxSeconds = 0.0;

	/** Exactly one for a DamageOverTime status, and none for any other kind. */
	UPROPERTY()
	TArray<FVeyraDamageOverTimeTuning> DamageOverTime;

	/** Seconds each remaining stack lasts once its duration runs out, for a status that loses one at a time; 0 for none. */
	UPROPERTY()
	double StackDecaySeconds = 0.0;
};

/** Where an area is placed. */
UENUM()
enum class EVeyraAreaOrigin : uint8
{
	/** On the caster, facing the cast's point. */
	Caster,
	/** On the cast's ground point, facing away from the caster. */
	TargetPoint,
};

/** A group whose shields from one caster together hold at most a share of its Max Health on a unit (ADR-009 §3). */
USTRUCT()
struct FVeyraShieldCapGroupTuning
{
	GENERATED_BODY()

	UPROPERTY()
	FVeyraContentId Id;

	/** The group's total, as a fraction of the caster's Max Health. */
	UPROPERTY()
	double TotalMaxHealthRatio = 0.0;
};

/** A shield an ability or passive grants its caster (Combat Bible §7, §51; ADR-009 §3). */
USTRUCT()
struct FVeyraShieldTuning
{
	GENERATED_BODY()

	/** The shield's identity: a new grant meets an active shield with it from the same caster as Reapply says. */
	UPROPERTY()
	FVeyraContentId Id;

	UPROPERTY()
	EVeyraShieldCategory Category = EVeyraShieldCategory::Universal;

	UPROPERTY()
	TArray<double> AmountByRank;

	/** Of the caster's Max Health, added to the amount. */
	UPROPERTY()
	double MaxHealthRatio = 0.0;

	UPROPERTY()
	double MagicPowerRatio = 0.0;

	UPROPERTY()
	double DurationSeconds = 0.0;

	UPROPERTY()
	EVeyraShieldReapply Reapply = EVeyraShieldReapply::Replace;

	/** The most a merged shield holds, as a fraction of the caster's Max Health; no less than one grant. */
	UPROPERTY()
	double MaxAmountMaxHealthRatio = 0.0;

	/** At most one. */
	UPROPERTY()
	TArray<FVeyraShieldCapGroupTuning> CapGroup;
};

/** One zone of an area: its shape and what it does. */
USTRUCT()
struct FVeyraAreaZoneTuning
{
	GENERATED_BODY()

	UPROPERTY()
	FVeyraShape Shape;

	UPROPERTY()
	FVeyraEffectBundleTuning Effects;

	/**
	 * At most one: a shield the caster gains once for each enemy Vanguard the zone catches, such as an
	 * ultimate's per-target shield (ADR-008 §9).
	 */
	UPROPERTY()
	TArray<FVeyraShieldTuning> CasterShieldPerVanguard;

	/**
	 * Status IDs from the statuses map the caster gains for each enemy Vanguard the zone catches, such
	 * as the state an ultimate enters when it lands on a Vanguard (No Quarter; ADR-008 §9).
	 */
	UPROPERTY()
	TArray<FVeyraContentId> CasterStatusesPerVanguard;
};

/**
 * Ordinary vision an area lights for its caster's side as it commits, which over Dense Fog senses
 * presence instead (ADR-016 §5), as Bryn's Sounding Flare's. 0 and 0 for an area that lights nothing.
 */
USTRUCT()
struct FVeyraAreaRevealTuning
{
	GENERATED_BODY()

	UPROPERTY()
	double Radius = 0.0;

	UPROPERTY()
	double DurationSeconds = 0.0;
};

/** What a lingering area shows its caster's side (ADR-018 §5). */
UENUM()
enum class EVeyraLingerSight : uint8
{
	/** Nothing. */
	None,
	/** Its shape is ordinary vision while it lasts: never True Sight, and nothing in Dense Fog. */
	Ordinary,
};

/** A delivered area that lasts, giving those inside it statuses by side (ADR-018 §5). */
USTRUCT()
struct FVeyraLingerTuning
{
	GENERATED_BODY()

	UPROPERTY()
	double DurationSeconds = 0.0;

	/** Seconds between the statuses it gives; it gives them first as it lands. */
	UPROPERTY()
	double PulseSeconds = 0.0;

	/** For its caster, while inside. */
	UPROPERTY()
	TArray<FVeyraContentId> CasterStatuses;

	/** For the allied Vanguards inside, its caster apart. */
	UPROPERTY()
	TArray<FVeyraContentId> AllyStatuses;

	/** For the enemy units inside. */
	UPROPERTY()
	TArray<FVeyraContentId> EnemyStatuses;

	UPROPERTY()
	EVeyraLingerSight Sight = EVeyraLingerSight::None;
};

/** An ability that hits the enemies in shapes at the caster or a ground point (ADR-008 §3). */
USTRUCT()
struct FVeyraAreaAbilityTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	UPROPERTY()
	FVeyraCastTuning Cast;

	UPROPERTY()
	EVeyraAreaOrigin Origin = EVeyraAreaOrigin::Caster;

	/** Seconds between Commit and the hit, with the area telegraphed; the caster is free meanwhile. */
	UPROPERTY()
	double DelaySeconds = 0.0;

	/** How many times the area hits over ChannelSeconds, the caster held in place; 1 and 0 hit once. */
	UPROPERTY()
	int32 ChannelTicks = 1;

	UPROPERTY()
	double ChannelSeconds = 0.0;

	UPROPERTY()
	FVeyraAreaRevealTuning Reveal;

	/** Innermost first: a unit takes the first zone that touches it, and no other. */
	UPROPERTY()
	TArray<FVeyraAreaZoneTuning> Zones;

	/** At most one: after it hits, the area lasts in its outermost zone's shape (ADR-018 §5). */
	UPROPERTY()
	TArray<FVeyraLingerTuning> Linger;

	/** Statuses the caster loses as it commits, whatever their stacks, as Break the Line spends Cadence (ADR-018 §6). */
	UPROPERTY()
	TArray<FVeyraContentId> ConsumesCasterStatuses;
};

/** Statuses a buff gives nearby allied Vanguards while it lasts (ADR-008 §9). */
USTRUCT()
struct FVeyraAuraTuning
{
	GENERATED_BODY()

	/** Units from the caster's centre to an ally's edge. */
	UPROPERTY()
	double Radius = 0.0;

	/** How long the aura lasts, in seconds. */
	UPROPERTY()
	double DurationSeconds = 0.0;

	/** How often allies in range are given the statuses again, in seconds. The statuses should outlast it. */
	UPROPERTY()
	double RefreshSeconds = 0.0;

	UPROPERTY()
	TArray<FVeyraContentId> AllyStatuses;
};

/**
 * Health a buff restores to its caster and to one allied Vanguard, the one near it that lacks the most
 * of its Health (ADR-015 §3), never above Max Health.
 */
USTRUCT()
struct FVeyraHealTuning
{
	GENERATED_BODY()

	/** At the caster's first Level. */
	UPROPERTY()
	double Amount = 0.0;

	/** Added for each Level of the caster's beyond the first, read at Commit. */
	UPROPERTY()
	double AmountPerLevel = 0.0;

	/** Units from the caster's centre to an ally's edge; 0 heals the caster alone. */
	UPROPERTY()
	double AllyRange = 0.0;

	/** Status IDs from the statuses map, put on each unit it heals. */
	UPROPERTY()
	TArray<FVeyraContentId> Statuses;
};

/** What casting a buff again does while it lasts. */
UENUM()
enum class EVeyraRecast : uint8
{
	/** Nothing: the cooldown applies. */
	None,
	/** It ends the buff early, at no cost (ADR-008 §9). */
	EndsEarly,
};

/** Whether a variant keeps its own cooldown or shares its slot's (ADR-018 §1). */
UENUM()
enum class EVeyraVariantCooldown : uint8
{
	/** Its own: casting it leaves the slot's own ability ready. */
	Own,
	/** One cooldown with the slot's own ability. */
	Shared,
};

/** While a buff lasts, a slot holds another ability (ADR-018 §1), as Dig In under The Last Volley. */
USTRUCT()
struct FVeyraVariantTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraAbilitySlot Slot = EVeyraAbilitySlot::Q;

	/** The variant: another ability's ID. It takes the slot's rank. */
	UPROPERTY()
	FVeyraContentId Ability;

	UPROPERTY()
	EVeyraVariantCooldown Cooldown = EVeyraVariantCooldown::Own;

	UPROPERTY()
	double DurationSeconds = 0.0;
};

/** An ability that buffs its caster, and optionally nearby allies (ADR-008 §3). */
USTRUCT()
struct FVeyraSelfBuffAbilityTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	UPROPERTY()
	FVeyraCastTuning Cast;

	/** Status IDs from the statuses map, put on the caster. */
	UPROPERTY()
	TArray<FVeyraContentId> Statuses;

	/** At most one. */
	UPROPERTY()
	TArray<FVeyraShieldTuning> Shields;

	/** At most one. */
	UPROPERTY()
	TArray<FVeyraAuraTuning> Aura;

	/** At most one. */
	UPROPERTY()
	TArray<FVeyraHealTuning> Heal;

	UPROPERTY()
	EVeyraRecast Recast = EVeyraRecast::None;

	/** Slots that hold another ability while the buff lasts (ADR-018 §1). */
	UPROPERTY()
	TArray<FVeyraVariantTuning> Variants;
};

/** How a projectile flies (Combat Bible §13). */
USTRUCT()
struct FVeyraProjectileTuning
{
	GENERATED_BODY()

	/** Units per second. */
	UPROPERTY()
	double Speed = 0.0;

	/** The projectile's own radius, in units: it hits a unit whose body it touches. */
	UPROPERTY()
	double Radius = 0.0;

	/** How far it flies before it ends, in units. */
	UPROPERTY()
	double Range = 0.0;
};

/** Which units stop a skillshot (Combat Bible §13; ADR-008 §9). */
UENUM()
enum class EVeyraSkillshotCollision : uint8
{
	/** The first enemy unit of any kind. */
	FirstEnemy,
	/** The first enemy Vanguard. It passes through other enemies, applying its pass-through effects to each. */
	FirstEnemyVanguard,
	/** Nothing: it hits every enemy on its path once, until its range or terrain ends it. */
	Pierce,
};

/** The caster's own movement as a skillshot fires: a recoil away from its aim (ADR-018 §6). */
USTRUCT()
struct FVeyraCasterDashTuning
{
	GENERATED_BODY()

	UPROPERTY()
	double Distance = 0.0;

	UPROPERTY()
	double Speed = 0.0;
};

/** An ability that fires a line projectile toward the cast's point (ADR-008 §3). Terrain stops it (ADR-008 §9). */
USTRUCT()
struct FVeyraSkillshotAbilityTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	UPROPERTY()
	FVeyraCastTuning Cast;

	UPROPERTY()
	FVeyraProjectileTuning Projectile;

	UPROPERTY()
	EVeyraSkillshotCollision Collision = EVeyraSkillshotCollision::FirstEnemy;

	/** On each unit the projectile hits. */
	UPROPERTY()
	FVeyraEffectBundleTuning Effects;

	/** On each unit a FirstEnemyVanguard projectile passes through; empty otherwise. */
	UPROPERTY()
	FVeyraEffectBundleTuning PassThroughEffects;

	/** At most one: the caster recoils away from the aim as it fires, as Kade's Reposition does. */
	UPROPERTY()
	TArray<FVeyraCasterDashTuning> CasterDash;
};

/** Which way a dash goes. */
UENUM()
enum class EVeyraDashDirection : uint8
{
	/** Toward the cast's point. */
	TowardPoint,
	/** Straight back from it, as a recoil (Bryn's Kickback). */
	AwayFromPoint,
};

/** An ability that moves its caster, with effects as it sets off and where it stops (ADR-008 §3; Combat Bible §9). */
USTRUCT()
struct FVeyraDashAbilityTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	UPROPERTY()
	FVeyraCastTuning Cast;

	UPROPERTY()
	EVeyraDashDirection Direction = EVeyraDashDirection::TowardPoint;

	/** Units. Terrain stops the dash sooner; it never crosses terrain (ADR-008 §9). */
	UPROPERTY()
	double Distance = 0.0;

	/** Units per second. */
	UPROPERTY()
	double Speed = 0.0;

	UPROPERTY()
	EVeyraDashContact Contact = EVeyraDashContact::None;

	/** Areas at the caster, facing the cast's point, as the dash sets off; innermost first. */
	UPROPERTY()
	TArray<FVeyraAreaZoneTuning> StartZones;

	/** On the enemy a StopAtFirstEnemy dash stops at. */
	UPROPERTY()
	FVeyraEffectBundleTuning ContactEffects;

	/** Status IDs put on the caster when it stops at an enemy. */
	UPROPERTY()
	TArray<FVeyraContentId> ContactSelfStatuses;
};

/** The other enemies an empowered attack hits, in the attacker's cleave shape (ADR-009 §5). */
USTRUCT()
struct FVeyraAttackCleaveTuning
{
	GENERATED_BODY()

	/** The fraction of the attack's damage each takes, above 0 and at most 1. */
	UPROPERTY()
	double DamageFraction = 0.0;

	/** Status IDs from the statuses map, put on each. */
	UPROPERTY()
	TArray<FVeyraContentId> Statuses;
};

/** An empowered attack's secondary impact behind its target: proc damage (ADR-009 §5). */
USTRUCT()
struct FVeyraSecondaryImpactTuning
{
	GENERATED_BODY()

	/** A higher priority replaces a lower one, such as a passive's impact; an attack has at most one. */
	UPROPERTY()
	int32 Priority = 0;

	/** Placed at the target, facing away from the attacker; the target itself is never hit by it. */
	UPROPERTY()
	FVeyraShape Shape;

	UPROPERTY()
	TArray<FVeyraDamageTuning> Damage;

	UPROPERTY()
	TArray<FVeyraContentId> Statuses;
};

/** An ability that empowers its caster's next basic attack, which stays a basic attack (ADR-008 §3; Combat Bible §17). */
USTRUCT()
struct FVeyraEmpoweredAttackAbilityTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	UPROPERTY()
	FVeyraCastTuning Cast;

	/** How long the empowerment waits for an attack, in seconds. */
	UPROPERTY()
	double DurationSeconds = 0.0;

	/** Joins the attack's own damage event, so the empowered attack is still one hit (Combat Bible §25). */
	UPROPERTY()
	TArray<FVeyraDamageTuning> Damage;

	/** Status IDs put on the target. */
	UPROPERTY()
	TArray<FVeyraContentId> Statuses;

	/** The fraction of the target's Armor the attack ignores, by rank: percentage penetration (Combat Bible §3). */
	UPROPERTY()
	TArray<double> ArmorPenetrationByRank;

	/** At most one. */
	UPROPERTY()
	TArray<FVeyraAttackCleaveTuning> Cleave;

	/** At most one. */
	UPROPERTY()
	TArray<FVeyraSecondaryImpactTuning> SecondaryImpact;
};

/** Rules every cast shares (Combat Bible §26). */
USTRUCT()
struct FVeyraCastingTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** The part of its cooldown a cast starts when interrupted before Commit. */
	UPROPERTY()
	double InterruptedCooldownFraction = 0.0;
};

/**
 * The Flux Spells a player may put in a spell slot (Battleground Bible §14; ADR-015 §3). Each is an
 * ordinary entry of one archetype map, with one value for its every rank list.
 */
USTRUCT()
struct FVeyraFluxSpellsTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	UPROPERTY()
	TArray<FVeyraContentId> Roster;
};

/** A volley's extra shot, earned while it lasts (ADR-018 §6). */
USTRUCT()
struct FVeyraVolleyBonusTuning
{
	GENERATED_BODY()

	/** An ally displacing an enemy that carries this status from the caster earns a shot, as Tracked does. */
	UPROPERTY()
	FVeyraContentId Status;

	/** At most this many, however many are earned. */
	UPROPERTY()
	int32 MaxShots = 0;
};

/**
 * A lane its caster fires into, shot by shot (ADR-018 §6): Kade's Kill Corridor. As it commits, its
 * slot holds its shot for a while; each shot is a skillshot fired within the lane, at its own cooldown.
 */
USTRUCT()
struct FVeyraVolleyAbilityTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	UPROPERTY()
	FVeyraCastTuning Cast;

	/** Each shot: a skillshot, whose cooldown is the time between shots. */
	UPROPERTY()
	FVeyraContentId Shot;

	UPROPERTY()
	int32 Shots = 0;

	/** How far either side of the lane's direction a shot may aim, in degrees. */
	UPROPERTY()
	double LaneHalfAngleDegrees = 0.0;

	/** How long the lane lasts, whatever shots are left. */
	UPROPERTY()
	double DurationSeconds = 0.0;

	/** For the caster while the lane lasts, as a stance that plants it. */
	UPROPERTY()
	TArray<FVeyraContentId> CasterStatuses;

	/** At most one. */
	UPROPERTY()
	TArray<FVeyraVolleyBonusTuning> Bonus;
};

USTRUCT()
struct FVeyraAbilitiesTuning
{
	GENERATED_BODY()

	/** The Abilities.json format this build reads (a schema version marker, not tuning). */
	static constexpr int32 SchemaVersion = 6;

	UPROPERTY()
	FVeyraCastingTuning Casting;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraStatusTuning> Statuses;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraTargetedDamageAbilityTuning> TargetedDamage;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraAreaAbilityTuning> Area;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraSelfBuffAbilityTuning> SelfBuff;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraSkillshotAbilityTuning> Skillshot;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraDashAbilityTuning> Dash;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraEmpoweredAttackAbilityTuning> EmpoweredAttack;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraVolleyAbilityTuning> Volley;

	UPROPERTY()
	FVeyraFluxSpellsTuning FluxSpells;
};

/** The Abilities domain's rules for its tuning (ADR-008 §3, §7). */
namespace VeyraAbilityRules
{
	/** A "ByRank" value at Rank (from 1): the one value for every rank, or the rank's own. 0 outside the list. */
	VEYRAABILITIES_API double ValueAtRank(TConstArrayView<double> ByRank, int32 Rank);

	/**
	 * Status Id as Combat applies it, from a source at SourceLevel: a damage-over-time status's ticks
	 * are fixed as it lands (Combat Bible §14's snapshot).
	 */
	VEYRAABILITIES_API FVeyraStatusSpec ToStatusSpec(const FVeyraContentId& Id, const FVeyraStatusTuning& Status, int32 SourceLevel = 1);

	/** An amount at Level: Base, plus PerLevel for each Level beyond the first (ADR-015 §3). */
	VEYRAABILITIES_API double AtLevel(double Base, double PerLevel, int32 Level);

	/**
	 * Problems with Tuning that a schema cannot express, each a JSON pointer and a message: rank lists
	 * of the wrong length, statuses and shapes out of range, references to undefined statuses, and an
	 * ID in more than one archetype map. RankCounts are the lengths a rank list may have besides 1.
	 */
	VEYRAABILITIES_API TArray<FString> Validate(const FVeyraAbilitiesTuning& Tuning, TConstArrayView<int32> RankCounts);

	/** The way Dash moves its caster for a cast facing CastDirection. */
	VEYRAABILITIES_API FVector DashHeading(const FVeyraDashAbilityTuning& Dash, const FVector& CastDirection);

	/** Whether any archetype map of Tuning defines Ability. */
	VEYRAABILITIES_API bool Defines(const FVeyraAbilitiesTuning& Tuning, const FVeyraContentId& Ability);

	/** Ability's cooldown at Rank before any Haste, from whichever archetype map defines it; 0 for none. */
	VEYRAABILITIES_API double CooldownSeconds(const FVeyraAbilitiesTuning& Tuning, const FVeyraContentId& Ability, int32 Rank);

	/**
	 * Problems with Ability as the ability of a slot with RankCount ranks: each of its rank lists must
	 * hold one value, or exactly RankCount (ADR-008 §3).
	 */
	VEYRAABILITIES_API TArray<FString> ValidateRanks(const FVeyraAbilitiesTuning& Tuning, const FVeyraContentId& Ability, int32 RankCount);
}
