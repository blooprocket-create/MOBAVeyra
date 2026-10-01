// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Absorption/VeyraAbsorptionLedger.h"
#include "Attacks/VeyraBasicAttackTypes.h"
#include "Slots/VeyraAbilitySlot.h"
#include "Content/VeyraContentId.h"
#include "Damage/VeyraDamageTypes.h"
#include "Movement/VeyraForcedMovementTypes.h"
#include "Shapes/VeyraShapes.h"
#include "Stats/VeyraStatBlock.h"
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

/** When a recast window opens (ADR-018 §1; ADR-030 §7). */
UENUM()
enum class EVeyraRecastCondition : uint8
{
	/** As every cast commits. */
	Always,
	/** As the cast commits, only if its target already held HeldStatus from the caster. */
	TargetHeld,
	/** Only if the cast's target dies, its kill credited to the caster, within FallsWithinSeconds. */
	TargetFalls,
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

	UPROPERTY()
	EVeyraRecastCondition OpensWhen = EVeyraRecastCondition::Always;

	/** TargetHeld: one status ID. Empty otherwise. */
	UPROPERTY()
	TArray<FVeyraContentId> HeldStatus;

	/** TargetFalls: how long after the cast the target's fall still opens it, in seconds. 0 otherwise. */
	UPROPERTY()
	double FallsWithinSeconds = 0.0;

	/** Always: how long after the cast commits it opens, in seconds, as a wall cools before it can be detonated (ADR-032 §5); 0 for at once. */
	UPROPERTY()
	double ArmingSeconds = 0.0;
};

/** Whether a cast needs its caster's companion on the battleground (ADR-034 §8). */
UENUM()
enum class EVeyraCompanionNeed : uint8
{
	None,
	/** Refused, as NoCompanion, while its caster has no living companion. */
	Living,
};

/** A cooldown a cast starts while its caster holds a status is scaled (ADR-034 §8), as Hell on a Leash shortens Hunt's. */
USTRUCT()
struct FVeyraCooldownWhileTuning
{
	GENERATED_BODY()

	/** A status ID from the statuses map, held from the caster itself. */
	UPROPERTY()
	FVeyraContentId Status;

	/** Above 0, at most 1. */
	UPROPERTY()
	double Multiplier = 1.0;
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

	/** At most one status ID: the cast takes only a target holding it from the caster (ADR-030 §7). */
	UPROPERTY()
	TArray<FVeyraContentId> TargetMustHold;

	/**
	 * At most one status ID: the cast refuses a target that holds it from the caster, the inverse of
	 * TargetMustHold, as Passing Step's lockout on a unit it just passed through (ADR-031 §8).
	 */
	UPROPERTY()
	TArray<FVeyraContentId> TargetMustNotHold;

	/**
	 * At most one: the fraction of its remaining cooldown a takedown its caster takes part in refunds; 1
	 * resets it, as Black Step's (ADR-031 §9).
	 */
	UPROPERTY()
	TArray<double> TakedownRefund;

	/**
	 * At most one: the share of its caster's current resource it also costs, above 0 and at most 1, paid at
	 * Commit with the cost by rank (Combat Bible §27; ADR-033 §3), as Rapid Discharge's share of Charge.
	 */
	UPROPERTY()
	TArray<double> CurrentResourceFraction;

	/** At most one: the resource, above 0, its caster must hold to cast it at all (ADR-033 §3). */
	UPROPERTY()
	TArray<double> MinimumResource;

	UPROPERTY()
	EVeyraCompanionNeed NeedsCompanion = EVeyraCompanionNeed::None;

	/** Each status its caster holds as it starts its cooldown scales the cooldown (ADR-034 §8). */
	UPROPERTY()
	TArray<FVeyraCooldownWhileTuning> CooldownWhile;

	/**
	 * At most one: another ability whose cooldown it shares, held under that ability's ID, as one Sea State's
	 * ability shares the other's (ADR-035 §1). The ability it names shares none itself.
	 */
	UPROPERTY()
	TArray<FVeyraContentId> CooldownOf;

	/** Status IDs whose holding refuses the cast, as HeldBack, as Tidebreaker's lock refuses Change the Weather (ADR-035 §2). */
	UPROPERTY()
	TArray<FVeyraContentId> RefusedWhile;
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

	/**
	 * From Abilities.json's statuses: given a unit whose displacement collides with terrain, a Vanguard
	 * or a structure, where it stops (ADR-028 §3), as Battering Mass stuns.
	 */
	UPROPERTY()
	TArray<FVeyraContentId> CollisionStatuses;
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

/** An effect bundle's damage against one kind of unit, multiplied (ADR-018 §6). */
USTRUCT()
struct FVeyraUnitKindMultiplierTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraUnitKind Kind = EVeyraUnitKind::Wildlife;

	/** At least 1: extra effectiveness only; never against structures. */
	UPROPERTY()
	double Multiplier = 1.0;
};

/** Whether a reaction removes the status it reacted to (ADR-026 §1). */
UENUM()
enum class EVeyraReactionConsume : uint8
{
	Keep,
	Consume,
};

/** Whether a reaction's damage counts once or once per stack the target held (ADR-026 §1). */
UENUM()
enum class EVeyraReactionScaling : uint8
{
	Once,
	PerStack,
};

/**
 * A burst a reaction sets off around its target (ADR-034 §6), as Witchfire's explosion: damage and
 * statuses for the caster's other enemies within its shape, centred on the target, which it spares.
 */
USTRUCT()
struct FVeyraReactionBurstTuning
{
	GENERATED_BODY()

	UPROPERTY()
	FVeyraShape Shape;

	/** One component per damage type at most. */
	UPROPERTY()
	TArray<FVeyraDamageTuning> Damage;

	UPROPERTY()
	TArray<FVeyraContentId> Statuses;
};

/**
 * What a hit adds when its target holds a status (ADR-026 §1), as Rupture's burst on Splinters or
 * Flash Cure's stun on an Unstable target. It reads the statuses the target held as the hit landed.
 */
USTRUCT()
struct FVeyraReactionTuning
{
	GENERATED_BODY()

	/** The status the target must hold, from any source. */
	UPROPERTY()
	FVeyraContentId Status;

	UPROPERTY()
	EVeyraReactionConsume Consume = EVeyraReactionConsume::Keep;

	/** At most one: extra damage, once or per stack held. */
	UPROPERTY()
	TArray<FVeyraDamageTuning> Damage;

	UPROPERTY()
	EVeyraReactionScaling Scaling = EVeyraReactionScaling::Once;

	/** Extra statuses for the target. */
	UPROPERTY()
	TArray<FVeyraContentId> Statuses;

	/** Statuses of the bundle's own that this reaction takes the place of. */
	UPROPERTY()
	TArray<FVeyraContentId> Replaces;

	/** At most one: a burst around the target as it reacts (ADR-034 §6). */
	UPROPERTY()
	TArray<FVeyraReactionBurstTuning> Burst;
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

	/** Damage multiplied against some kinds of unit (ADR-018 §6), as Gorraveth's against wildlife; one entry per kind. */
	UPROPERTY()
	TArray<FVeyraUnitKindMultiplierTuning> UnitKindMultipliers;

	/**
	 * Status IDs that spare a unit this bundle's displacement: one displacement per target per cast, as
	 * Raska's Hound spares what her landing knocked up (Roster Bible §1).
	 */
	UPROPERTY()
	TArray<FVeyraContentId> DisplacementUnlessStatuses;

	/** What the hit adds against statuses its target holds (ADR-026 §1). */
	UPROPERTY()
	TArray<FVeyraReactionTuning> Reactions;
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

	/** A DirectionalDamageReduction's guarded arc, in degrees; 0 for any other kind. */
	UPROPERTY()
	double ArcDegrees = 0.0;

	/** An AttackDamageAmplification's unit kinds, empty for all; empty for any other kind. */
	UPROPERTY()
	TArray<EVeyraUnitKind> UnitKinds;

	/**
	 * At most one: the status this becomes when an application brings it to its most stacks, from the
	 * same source, as Splinters become Fractured (ADR-026 §2).
	 */
	UPROPERTY()
	TArray<FVeyraContentId> AtMaxStacks;

	/** The kinds of unit it lands on, empty for every kind (ADR-026 §2). */
	UPROPERTY()
	TArray<EVeyraUnitKind> LandsOn;

	/** How many of its holder's basic attacks it lasts, each that commits spending one (ADR-033 §4); 0 for a status attacks do not spend. */
	UPROPERTY()
	int32 AttackCharges = 0;
};

/** Where an area is placed. */
UENUM()
enum class EVeyraAreaOrigin : uint8
{
	/** On the caster, facing the cast's point. */
	Caster,
	/** On the cast's ground point, facing away from the caster. */
	TargetPoint,
	/**
	 * On the caster's own lingering area of the ability originAbility names, facing as it does; that area
	 * ends at once, without its end effects (ADR-028 §5), as Anchor's recast rips it up.
	 */
	CastersLingeringArea,
	/**
	 * On the caster's own standing marker of the ability originAbility names, facing as it does; that
	 * marker ends at once (ADR-032 §6), as Shatterforge detonates its wall.
	 */
	CastersMarker,
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

/**
 * What a shield gives its holder once it has absorbed enough (ADR-027 §5), as Windward's second speed
 * burst: the statuses, once, from the shield's provider.
 */
USTRUCT()
struct FVeyraAbsorbedRewardTuning
{
	GENERATED_BODY()

	/** Of what the grant gave, the share it must absorb; above 0, at most 1. */
	UPROPERTY()
	double Fraction = 0.0;

	/** From Abilities.json's statuses. */
	UPROPERTY()
	TArray<FVeyraContentId> Statuses;
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

	/** At most one: what its holder gains once it has absorbed enough (ADR-027 §5). */
	UPROPERTY()
	TArray<FVeyraAbsorbedRewardTuning> AbsorbedReward;
};

/** Whom among its caster's side a zone's ally effects reach (ADR-035 §4). */
UENUM()
enum class EVeyraAllyReach : uint8
{
	/** The allied Vanguards in its shape, its caster apart. */
	OthersOnly,
	/** The allied Vanguards in its shape, its caster among them. */
	CasterToo,
};

/** What a zone does for the allied Vanguards in its shape (ADR-035 §4): its caster's heal, and statuses. */
USTRUCT()
struct FVeyraZoneAllyEffectsTuning
{
	GENERATED_BODY()

	/** Health each ally regains, by rank, before Magic Power; 0 with no ratio for no heal. */
	UPROPERTY()
	TArray<double> HealByRank;

	/** Added to the heal for each point of its caster's Magic Power at Commit; at least 0. */
	UPROPERTY()
	double HealMagicPowerRatio = 0.0;

	/** Status IDs from the statuses map. */
	UPROPERTY()
	TArray<FVeyraContentId> Statuses;

	UPROPERTY()
	EVeyraAllyReach Reach = EVeyraAllyReach::OthersOnly;
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

	/** At most one: what it does for its caster's allies in its shape (ADR-035 §4). */
	UPROPERTY()
	TArray<FVeyraZoneAllyEffectsTuning> AllyEffects;

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

/** A movement field a lingering area holds while it stands (ADR-033 §5), as Magnetic Field's. */
USTRUCT()
struct FVeyraMovementFieldTuning
{
	GENERATED_BODY()

	/** How far, at most, it moves an enemy forced move's end toward its centre; above 0. */
	UPROPERTY()
	double Pull = 0.0;
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

	/**
	 * At most one: what each pulse after it lands does to the enemy units inside, reactions included
	 * (ADR-026 §4). Its zones are what it does as it lands.
	 */
	UPROPERTY()
	TArray<FVeyraEffectBundleTuning> PulseEffects;

	/** At most one: what it does to the enemy units inside as it ends, measured from its centre (ADR-026 §4). */
	UPROPERTY()
	TArray<FVeyraEffectBundleTuning> EndEffects;

	/**
	 * How long before its end the presentation marks it, so a rupture is readable (Roster Bible §17):
	 * above 0 exactly when it has EndEffects, and no longer than it lasts.
	 */
	UPROPERTY()
	double EndWarningSeconds = 0.0;

	/**
	 * At most one: a field it holds while it stands, bending its caster's enemies' dashes and displacements
	 * toward its centre (ADR-033 §5). Its area's outermost zone is then a circle, the field's extent.
	 */
	UPROPERTY()
	TArray<FVeyraMovementFieldTuning> MovementField;
};

/**
 * Health an area's caster restores from the units it hits (ADR-018 §6), as Gorraveth's Furnace Rake
 * from wildlife: a share of its Max Health per unit hit, up to a share for the whole cast.
 */
USTRUCT()
struct FVeyraHealOnHitTuning
{
	GENERATED_BODY()

	/** The kinds of unit that count; empty for every kind. */
	UPROPERTY()
	TArray<EVeyraUnitKind> UnitKinds;

	/** Of the caster's Max Health, for each unit hit; above 0. */
	UPROPERTY()
	double MaxHealthRatioPerHit = 0.0;

	/** Of the caster's Max Health, the most one cast restores; at least MaxHealthRatioPerHit. */
	UPROPERTY()
	double CapMaxHealthRatio = 0.0;
};

/**
 * A delay a delayed area takes instead while its point lies inside its caster's lingering area of
 * another ability (ADR-026 §4), as Flash Cure's inside CODE BLACK.
 */
USTRUCT()
struct FVeyraAreaDelayWithinTuning
{
	GENERATED_BODY()

	/** An area ability that lingers. */
	UPROPERTY()
	FVeyraContentId Ability;

	/** Above 0. */
	UPROPERTY()
	double DelaySeconds = 0.0;
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

	/** For the CastersLingeringArea and CastersMarker origins, exactly one: the ability whose lingering area or marker it lands on; else none. */
	UPROPERTY()
	TArray<FVeyraContentId> OriginAbility;

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

	/**
	 * Whether the caster may move while it channels (ADR-018 §6), as Gorraveth's Furnace Rake: Free
	 * places each tick of a caster-centred area where the caster stands then, facing its way.
	 */
	UPROPERTY()
	EVeyraCastMovement ChannelMovement = EVeyraCastMovement::Locked;

	/** Status IDs put on the caster as it commits, such as the Slow it channels under. */
	UPROPERTY()
	TArray<FVeyraContentId> CasterStatuses;

	/** At most one: Health the caster restores from the units it hits, capped for the cast. */
	UPROPERTY()
	TArray<FVeyraHealOnHitTuning> HealOnHit;

	/** For a delayed area: the first entry whose lingering area holds the area's point sets its delay instead. */
	UPROPERTY()
	TArray<FVeyraAreaDelayWithinTuning> DelayWithin;
};

/**
 * Statuses a buff gives nearby allied Vanguards while it lasts (ADR-008 §9), and optionally puts on the
 * enemy units near it (ADR-018 §6), as Patch's The Thing Inside.
 */
USTRUCT()
struct FVeyraAuraTuning
{
	GENERATED_BODY()

	/** Units from the caster's centre to a unit's edge. */
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

	/** Put on each living enemy unit in range at every refresh, never a structure or a ward. */
	UPROPERTY()
	TArray<FVeyraContentId> EnemyStatuses;

	/** Put on each allied Fluxborn in range at every refresh, as Full Grid overclocks them (ADR-033 §6). */
	UPROPERTY()
	TArray<FVeyraContentId> AllyFluxbornStatuses;
};

/**
 * What a buff drains of its caster's resource while it lasts (ADR-033 §6), as Full Grid's Charge: so much a
 * second, taken every interval, for at most so long; it ends early when the resource runs out.
 */
USTRUCT()
struct FVeyraDrainTuning
{
	GENERATED_BODY()

	UPROPERTY()
	double PerSecond = 0.0;

	UPROPERTY()
	double IntervalSeconds = 0.0;

	/** The longest the buff lasts, drained or not. */
	UPROPERTY()
	double MaxSeconds = 0.0;
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

/** Temporary Health a self-buff grants its caster (Combat Bible §7; ADR-018 §6), as Patch's The Thing Inside. */
USTRUCT()
struct FVeyraTemporaryHealthTuning
{
	GENERATED_BODY()

	/** One value for every rank, or one per rank. */
	UPROPERTY()
	TArray<double> AmountByRank;

	/** And this share of the caster's Max Health. */
	UPROPERTY()
	double MaxHealthRatio = 0.0;

	/** Above 0. */
	UPROPERTY()
	double DurationSeconds = 0.0;
};

/**
 * What a self-buff does as it ends (ADR-018 §6), as Patch's Play Dead's Fear pulse: a status on each
 * enemy unit within Radius of its caster, lasting longer for each hit the caster took while the buff
 * lasted, up to its most.
 */
USTRUCT()
struct FVeyraEndPayloadTuning
{
	GENERATED_BODY()

	/** When it comes, in seconds after the cast: as the buff ends. Above 0. */
	UPROPERTY()
	double AfterSeconds = 0.0;

	/** Units from the caster's centre to an enemy's edge; above 0. */
	UPROPERTY()
	double Radius = 0.0;

	/** A status ID from the statuses map. */
	UPROPERTY()
	FVeyraContentId Status;

	/**
	 * At most one, AwayFromOrigin: each enemy the payload reaches is also pushed away from the buff's
	 * holder (ADR-028 §6), as Overcapacity vents.
	 */
	UPROPERTY()
	TArray<FVeyraDisplacementTuning> Displacement;

	/** How long the status lasts with no hit taken; above 0. */
	UPROPERTY()
	double BaseSeconds = 0.0;

	/** Added for each hit taken; at least 0. */
	UPROPERTY()
	double SecondsPerHit = 0.0;

	/** The most it lasts; at least BaseSeconds. */
	UPROPERTY()
	double MaxSeconds = 0.0;

	/** The hits the caster must take for it to come at all, as Countersteer's counter needs a blocked hit; 0 for always. */
	UPROPERTY()
	int32 MinHits = 0;
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

/**
 * A secondary impact a buff's caster's basic attacks offer while it lasts (ADR-027 §3), as OPEN ROAD!'s
 * shots pierce behind their target.
 */
USTRUCT()
struct FVeyraBuffAttackImpactTuning
{
	GENERATED_BODY()

	/** How long its attacks offer the impact, in seconds; above 0. */
	UPROPERTY()
	double Seconds = 0.0;

	UPROPERTY()
	FVeyraSecondaryImpactTuning Impact;
};

/** Who a self-buff buffs (ADR-027 §4). */
UENUM()
enum class EVeyraBuffRecipient : uint8
{
	/** Its caster. */
	Caster,
	/** An allied Vanguard the cast names within its range, or else its caster. */
	CasterOrAlly,
};

/** How a placed marker looks to its owner's enemies (ADR-030 §5). */
UENUM()
enum class EVeyraMarkerLook : uint8
{
	/** As what it is. */
	Plain,
	/** As its owner: its owner's body, bars and minimap mark, a decoy. */
	AsOwner,
};

/**
 * A placed marker a self-buff leaves where its caster stood (ADR-030 §5), as Tavi's illusion. Casting
 * the buff again while it stands recalls it; recalled or destroyed, it bursts with BurstZones around it.
 */
USTRUCT()
struct FVeyraBuffMarkerTuning
{
	GENERATED_BODY()

	UPROPERTY()
	double LifetimeSeconds = 0.0;

	/** The hits that destroy it; 0 for one no one can target. */
	UPROPERTY()
	int32 HitsToDestroy = 0;

	UPROPERTY()
	EVeyraMarkerLook Look = EVeyraMarkerLook::Plain;

	/** Innermost first: zones that land around it as it is recalled or destroyed. */
	UPROPERTY()
	TArray<FVeyraAreaZoneTuning> BurstZones;
};

/** Whether a self-buff outlives its caster's companion (ADR-034 §7). */
UENUM()
enum class EVeyraCompanionDeath : uint8
{
	Stays,
	/** It ends as its caster's companion is banished, as Hell on a Leash ends if Nix is killed. */
	Ends,
};

/**
 * A chain between a self-buff's caster and its companion while the buff lasts (ADR-034 §7), as Hell on a
 * Leash's: enemies on the line take its effects at each pulse, each no more often than perEnemySeconds.
 */
USTRUCT()
struct FVeyraChainTuning
{
	GENERATED_BODY()

	/** How wide the line is, across it, in units. */
	UPROPERTY()
	double Width = 0.0;

	UPROPERTY()
	double PulseSeconds = 0.0;

	UPROPERTY()
	double PerEnemySeconds = 0.0;

	/** The caster's hit, from its rank and power at the cast. */
	UPROPERTY()
	FVeyraEffectBundleTuning Effects;
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

	/** At most one. */
	UPROPERTY()
	TArray<FVeyraTemporaryHealthTuning> TemporaryHealth;

	/** At most one. */
	UPROPERTY()
	TArray<FVeyraEndPayloadTuning> EndPayload;

	/** At most one: a secondary impact its recipient's basic attacks offer for a while (ADR-027 §3). */
	UPROPERTY()
	TArray<FVeyraBuffAttackImpactTuning> AttackSecondaryImpact;

	/**
	 * Who takes the buff: its statuses, shields, Temporary Health, heal, aura and attack impact
	 * (ADR-027 §4). An ally's buff has a cast range, and holds no variants, end payload or stance.
	 */
	UPROPERTY()
	EVeyraBuffRecipient Recipient = EVeyraBuffRecipient::Caster;

	/**
	 * Zones that land once on its recipient as it takes the buff, facing away from the caster, and hit
	 * the enemies there, as ROOM TO BREATHE pushes them out (ADR-027 §8). Innermost first.
	 */
	UPROPERTY()
	TArray<FVeyraAreaZoneTuning> RecipientZones;

	/** At most one: a placed marker it leaves where its caster stood (ADR-030 §5). */
	UPROPERTY()
	TArray<FVeyraBuffMarkerTuning> Marker;

	/** At most one: what it drains of its caster's resource while it lasts; it ends as the resource runs out (ADR-033 §6). */
	UPROPERTY()
	TArray<FVeyraDrainTuning> Drain;

	/**
	 * Status IDs its recipient holds while the buff's shield holds, lasting no longer than the shield, and
	 * removed as it breaks or runs out (ADR-032 §3), as Tempered Shell's resistances. They need a shield.
	 */
	UPROPERTY()
	TArray<FVeyraContentId> ShieldHolds;

	/** Innermost first: zones that land around its recipient as the buff's shield breaks or runs out (ADR-032 §3). They need a shield. */
	UPROPERTY()
	TArray<FVeyraAreaZoneTuning> ShieldEndZones;

	/**
	 * Status IDs its caster's companion holds while the buff lasts (ADR-034 §7), as Nix's true form. They
	 * need the buff's own statuses, whose first marks how long the buff lasts.
	 */
	UPROPERTY()
	TArray<FVeyraContentId> CompanionStatuses;

	UPROPERTY()
	EVeyraCompanionDeath CompanionDeath = EVeyraCompanionDeath::Stays;

	/** At most one: a chain between its caster and its companion while it lasts (ADR-034 §7). */
	UPROPERTY()
	TArray<FVeyraChainTuning> Chain;
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

/**
 * A shot that comes back (ADR-030 §8): striking a unit that already held Status from its caster, it flies
 * back to the caster at Speed, and reaching them refunds CooldownRefund of the ability's remaining cooldown.
 */
USTRUCT()
struct FVeyraReturnShotTuning
{
	GENERATED_BODY()

	UPROPERTY()
	FVeyraContentId Status;

	UPROPERTY()
	double Speed = 0.0;

	UPROPERTY()
	double CooldownRefund = 0.0;
};

/**
 * A skillshot its caster's standing marker throws too, from where it stands toward the same point
 * (ADR-031 §6), as False Body's Shadow Needle.
 */
USTRUCT()
struct FVeyraSkillshotMimicTuning
{
	GENERATED_BODY()

	/** The ability whose marker throws. */
	UPROPERTY()
	FVeyraContentId MarkerAbility;

	/** What a unit both shots strike takes from the second, instead of the full effects. */
	UPROPERTY()
	FVeyraEffectBundleTuning RepeatEffects;
};

/**
 * A wall a skillshot leaves where its flight ends (ADR-032 §4), across its path: a placed marker of its
 * caster's, of the skillshot's own ID, that blocks units of both teams, dashes and line projectiles as
 * terrain does. One stands at a time.
 */
USTRUCT()
struct FVeyraWallTuning
{
	GENERATED_BODY()

	/** Across the path, in units. */
	UPROPERTY()
	double Length = 0.0;

	/** Along the path, in units. */
	UPROPERTY()
	double Thickness = 0.0;

	UPROPERTY()
	double LifetimeSeconds = 0.0;
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

	/** At most one: the shot flies back to its caster from a unit that already held a status of theirs (ADR-030 §8). */
	UPROPERTY()
	TArray<FVeyraReturnShotTuning> ReturnIfHeld;

	/** At most one: its caster's marker throws it too (ADR-031 §6). */
	UPROPERTY()
	TArray<FVeyraSkillshotMimicTuning> Mimic;

	/** At most one: the wall it leaves where its flight ends (ADR-032 §4). */
	UPROPERTY()
	TArray<FVeyraWallTuning> EndWall;
};

/** Which way a dash goes. */
UENUM()
enum class EVeyraDashDirection : uint8
{
	/** Toward the cast's point. */
	TowardPoint,
	/** Straight back from it, as a recoil (Bryn's Kickback). */
	AwayFromPoint,
	/**
	 * Straight back from the unit its caster holds on to, letting go of it (ADR-018 §2), as Patch's
	 * Bear Hug throw. Refused unless the caster holds on to one; the cast's point does not matter.
	 */
	AwayFromHost,
	/**
	 * Through the enemy unit the cast names, to Distance beyond it (ADR-030 §6), as Tavi's Tag!. The unit
	 * takes the contact effects as the dash sets off.
	 */
	ThroughTarget,
};

/** What a dash cast during one of its caster's own dashes does (ADR-031 §7). */
UENUM()
enum class EVeyraDuringDash : uint8
{
	/** It is refused until the dash under way ends. */
	Refused,
	/** It takes over: the dash under way ends where the caster is, and this one begins. */
	TakesOver,
};

/** Whether a dash leaves its caster's ride first (Combat Bible §56, "Leaving"). */
UENUM()
enum class EVeyraRideExit : uint8
{
	/** It rides on, if it rides. */
	Stay,
	/** It separates rider and vehicle first, as Bail Out and Last Exit do; the vehicle goes on without it. */
	Leave,
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

	/**
	 * On the unit an AwayFromHost dash lets go of, from where the caster sets off, as the throw's
	 * stumble; no effects for any other dash.
	 */
	UPROPERTY()
	FVeyraEffectBundleTuning HostEffects;

	/** Areas where the dash lands, facing its way, as Ravine Bound's; innermost first. None when a displacement cuts it short. */
	UPROPERTY()
	TArray<FVeyraAreaZoneTuning> EndZones;

	/** Cast during one of its caster's own dashes, as Flowing Cut during Passing Step (ADR-031 §7). */
	UPROPERTY()
	EVeyraDuringDash DuringDash = EVeyraDuringDash::Refused;

	UPROPERTY()
	EVeyraRideExit RideExit = EVeyraRideExit::Stay;

	/** ThroughTarget: the kinds of unit it may pass through, as Passing Step's (Roster Bible §15); empty for any enemy unit, and empty for any other dash. */
	UPROPERTY()
	TArray<EVeyraUnitKind> TargetKinds;
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

	/** How many basic attacks it empowers, at least 1; each spends one (ADR-027 §2). */
	UPROPERTY()
	int32 Attacks = 1;

	/** What the empowered attacks' windups are multiplied by, in (0, 1]; their interval is unchanged. */
	UPROPERTY()
	double WindupScale = 1.0;

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

/**
 * An ability that tethers an enemy to its caster (Combat Bible §43; ADR-018), as Patch's Don't Leave
 * Me: while it holds, the caster's side sees the target; stretched beyond its range, it may snap the
 * target back toward the caster once, and ends.
 */
USTRUCT()
struct FVeyraTetherAbilityTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	UPROPERTY()
	FVeyraCastTuning Cast;

	/** The kinds of unit it may tether; empty for any hostile unit. Never Structure (Combat Bible §33). */
	UPROPERTY()
	TArray<EVeyraUnitKind> TargetKinds;

	/** Edge to edge, in units; beyond it the tether stretches. Above 0. */
	UPROPERTY()
	double MaxRange = 0.0;

	/** Above 0. */
	UPROPERTY()
	double DurationSeconds = 0.0;

	/** Stretched, the pull toward the caster, in units before Displacement Resistance; 0 for none. */
	UPROPERTY()
	double SnapDistance = 0.0;

	/** The pull's units per second: above 0 with a pull, 0 without. */
	UPROPERTY()
	double SnapSpeed = 0.0;

	/** Status IDs held on the target while the tether lasts. */
	UPROPERTY()
	TArray<FVeyraContentId> TargetStatuses;
};

/**
 * An ability that leaps its caster at an enemy and holds on to it (ADR-018 §2), as Patch's Bear Hug:
 * the leap is a dash toward the target; ending within reach of it, the caster attaches for a while,
 * and the host takes the host effects and holds the host statuses meanwhile. A leap that ends out of
 * reach does nothing more.
 */
USTRUCT()
struct FVeyraAttachAbilityTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	UPROPERTY()
	FVeyraCastTuning Cast;

	/** The kinds of unit it may hold on to; empty for any hostile unit. Never Structure (Combat Bible §33). */
	UPROPERTY()
	TArray<EVeyraUnitKind> TargetKinds;

	/** The leap's units per second; above 0. */
	UPROPERTY()
	double LeapSpeed = 0.0;

	/** Edge to edge, in units, how near the target must be as the leap ends for the caster to take hold. */
	UPROPERTY()
	double ReachOnArrival = 0.0;

	/** How long it holds on, in seconds; above 0. */
	UPROPERTY()
	double AttachSeconds = 0.0;

	/** Status IDs held on the host while the caster holds on. */
	UPROPERTY()
	TArray<FVeyraContentId> HostStatuses;

	/** On the host as the caster takes hold. */
	UPROPERTY()
	FVeyraEffectBundleTuning HostEffects;
};

/** One slot a ride holds for its duration, and the mounted action it holds (Combat Bible §56). */
USTRUCT()
struct FVeyraRideSlotTuning
{
	GENERATED_BODY()

	/** Q, W or E: a basic ability's slot, whose rank the mounted action shares. */
	UPROPERTY()
	EVeyraAbilitySlot Slot = EVeyraAbilitySlot::Q;

	UPROPERTY()
	FVeyraContentId Ability;
};

/**
 * An ambush (ADR-030 §9), as Tavi's Ready or Not!: cast on an enemy Vanguard its caster damaged,
 * crowd-controlled or debuffed within RecentSeconds, the caster vanishes, Invisible and Untargetable,
 * for VanishSeconds, then blinks BesideDistance from its target, if it still lives, and strikes.
 */
USTRUCT()
struct FVeyraAmbushAbilityTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	UPROPERTY()
	FVeyraCastTuning Cast;

	UPROPERTY()
	double RecentSeconds = 0.0;

	UPROPERTY()
	double VanishSeconds = 0.0;

	/** On the caster while it is gone, ended as it strikes: its Invisible and Untargetable statuses. */
	UPROPERTY()
	TArray<FVeyraContentId> VanishStatuses;

	/** How far from its target's edge it reappears. */
	UPROPERTY()
	double BesideDistance = 0.0;

	/** On the target as it strikes. */
	UPROPERTY()
	FVeyraEffectBundleTuning Effects;
};

/**
 * An ability that puts its caster in a ride state (Combat Bible §56; ADR-018 §7), as Raska's Kickstart
 * and NO BRAKES: a set Movement Speed and a limited turn rate, passing through units and unable to
 * attack, its mounted actions in their slots with their own cooldowns, and statuses held meanwhile.
 * It ends with its time, its rider's death, or a dash that leaves it; on every end its vehicle, if it
 * has one, goes on without its rider as a skillshot along the rider's heading.
 */
USTRUCT()
struct FVeyraRideAbilityTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	UPROPERTY()
	FVeyraCastTuning Cast;

	/** The rider's Movement Speed, set rather than added; above 0. */
	UPROPERTY()
	double SetSpeed = 0.0;

	/** How fast its heading turns, in degrees per second; above 0. */
	UPROPERTY()
	double TurnRateDegreesPerSecond = 0.0;

	/** Above 0. */
	UPROPERTY()
	double DurationSeconds = 0.0;

	/** Seconds over which the rider slows back to its ordinary speed after; at least 0. */
	UPROPERTY()
	double DecaySeconds = 0.0;

	/** Status IDs held on the rider while it rides, such as its larger body. */
	UPROPERTY()
	TArray<FVeyraContentId> RiderStatuses;

	/** The mounted actions, one per slot. */
	UPROPERTY()
	TArray<FVeyraRideSlotTuning> Mounted;

	/** At most one: the skillshot its vehicle goes on as, along the rider's heading, as the ride ends. */
	UPROPERTY()
	TArray<FVeyraContentId> Vehicle;

	/** Innermost first: zones that erupt where the rider is as the ride ends, on every end but death, as the rider's hit (ADR-035 §3). */
	UPROPERTY()
	TArray<FVeyraAreaZoneTuning> CrashZones;
};

/** An ability that ends its caster's ride at once (ADR-035 §3), as Breaking Wave's recast, which crashes it. */
USTRUCT()
struct FVeyraDismountAbilityTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	UPROPERTY()
	FVeyraCastTuning Cast;
};

/** An ability that places one of its caster's markers at a point (ADR-031 §4), as Angeru's False Body. */
USTRUCT()
struct FVeyraPlacementAbilityTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	UPROPERTY()
	FVeyraCastTuning Cast;

	/** The marker it places: its lifetime, the hits that destroy it and how it looks. It bursts with nothing. */
	UPROPERTY()
	FVeyraBuffMarkerTuning Marker;
};

/** Where a blink takes its caster (ADR-031 §5). */
UENUM()
enum class EVeyraBlinkTo : uint8
{
	/** Beside an enemy unit of its target kinds, which takes its effects. */
	EnemyUnit,
	/** To its caster's standing marker from its marker ability. */
	OwnMarker,
	/** Either, as the cast names the enemy or the marker. */
	EnemyUnitOrOwnMarker,
	/** To its caster's living companion (ADR-034 §5), within its cast range when it has one. */
	OwnCompanion,
};

/** What becomes of the marker a blink goes to (ADR-031 §5). */
UENUM()
enum class EVeyraBlinkSwap : uint8
{
	/** It stays where it stands. */
	None,
	/** It takes the caster's old place: the two exchange positions, as False Body's swap or Cross the Chain. */
	Swap,
};

/** An ability that blinks its caster beside an enemy unit or to its own marker (ADR-031 §5), as Black Step. */
USTRUCT()
struct FVeyraBlinkAbilityTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** Its range reaches the unit or marker it names; 0 lets a blink to its own marker reach it anywhere. */
	UPROPERTY()
	FVeyraCastTuning Cast;

	UPROPERTY()
	EVeyraBlinkTo To = EVeyraBlinkTo::EnemyUnit;

	/** At most one: the ability whose marker it may go to. */
	UPROPERTY()
	TArray<FVeyraContentId> MarkerAbility;

	/** The kinds of enemy unit it may name; any when empty. */
	UPROPERTY()
	TArray<EVeyraUnitKind> TargetKinds;

	/** How far from an enemy's edge it lands, on the caster's side of it. */
	UPROPERTY()
	double BesideDistance = 0.0;

	UPROPERTY()
	EVeyraBlinkSwap Swap = EVeyraBlinkSwap::None;

	/** On the enemy it blinks beside. */
	UPROPERTY()
	FVeyraEffectBundleTuning Effects;

	/** Innermost first: zones that erupt where its caster departed, as its caster's hit (ADR-034 §5). */
	UPROPERTY()
	TArray<FVeyraAreaZoneTuning> DepartureZones;

	/**
	 * Innermost first: zones that erupt where its caster's companion departed, as the companion's hit, from
	 * the companion's power (ADR-034 §5). Only for a swap with the companion.
	 */
	UPROPERTY()
	TArray<FVeyraAreaZoneTuning> CompanionDepartureZones;
};

/** One slot a stance holds, and the ability it holds there (ADR-031 §3). */
USTRUCT()
struct FVeyraStanceSlotTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraAbilitySlot Slot = EVeyraAbilitySlot::Q;

	/** Another ability's ID: the slot's own ability while the stance lasts. It takes the slot's rank. */
	UPROPERTY()
	FVeyraContentId Ability;
};

/**
 * An ability that swaps its caster's own abilities in some slots for another set, and back (ADR-031 §3),
 * as Angeru's Forsake the Schools. Each set keeps its own cooldowns.
 */
USTRUCT()
struct FVeyraStanceAbilityTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	UPROPERTY()
	FVeyraCastTuning Cast;

	/** The slots it holds, each once, with what it holds there. */
	UPROPERTY()
	TArray<FVeyraStanceSlotTuning> Slots;
};

/** What a command has its caster's companion do (ADR-034 §5). */
UENUM()
enum class EVeyraCompanionOrder : uint8
{
	/** Leap to the cast's point, land its zones there as its own hit, and hold the point. */
	Hold,
	/** Give up its hold and follow its owner again. */
	Recall,
	/**
	 * Form the caster's summoned companion for a while, bound to the unit the cast names: escorting an ally or
	 * hunting an enemy (ADR-035 §5). Cast while it lives, it is redirected instead.
	 */
	Summon,
	/** Bind the caster's living summoned companion to the unit the cast names, keeping its time (ADR-035 §5). */
	Redirect,
};

/** Which unit a summon or redirect binds the companion to (ADR-035 §5); None for the other orders. */
UENUM()
enum class EVeyraCompanionBind : uint8
{
	None,
	/** An allied Vanguard, its caster among them, whom it escorts. */
	Ally,
	/** An enemy unit, which it hunts. */
	Enemy,
};

/** An ability that commands its caster's companion (ADR-034 §5), as Marek's Hunt and its recall. */
USTRUCT()
struct FVeyraCommandAbilityTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** A hold's cast range reaches its point; its recast window offers the recall. */
	UPROPERTY()
	FVeyraCastTuning Cast;

	UPROPERTY()
	EVeyraCompanionOrder Order = EVeyraCompanionOrder::Hold;

	/** Hold: how fast the companion leaps to the point, in units per second; 0 for a recall. */
	UPROPERTY()
	double LeapSpeed = 0.0;

	/** Hold: how long it holds the point once it lands, in seconds; 0 for a recall. */
	UPROPERTY()
	double HoldSeconds = 0.0;

	/** Hold, innermost first: zones that land where it lands, as its own hit, from its own power; none for a recall. */
	UPROPERTY()
	TArray<FVeyraAreaZoneTuning> LandingZones;

	/** Summon: the one companion definition it forms (ADR-035 §5); none for the other orders. */
	UPROPERTY()
	TArray<FVeyraContentId> Companion;

	/** Summon: how long the companion stays, in seconds; 0 for the other orders. */
	UPROPERTY()
	double LifetimeSeconds = 0.0;

	/** Summon and Redirect: which unit the cast names; None for the other orders. */
	UPROPERTY()
	EVeyraCompanionBind BindTo = EVeyraCompanionBind::None;
};

/**
 * What a summoned companion does now and then for the ally it escorts (ADR-035 §5), as the Waterling in Calm:
 * a heal of its own, from its Magic Power, and statuses such as a little Movement Speed.
 */
USTRUCT()
struct FVeyraEscortTuning
{
	GENERATED_BODY()

	/** Seconds between its pulses, the first as it binds; above 0. */
	UPROPERTY()
	double PulseSeconds = 0.0;

	/** Health the ally regains each pulse, before Magic Power; at least 0. */
	UPROPERTY()
	double HealAmount = 0.0;

	/** Added to each heal for each point of the companion's own Magic Power; at least 0. */
	UPROPERTY()
	double HealMagicPowerRatio = 0.0;

	/** Status IDs the ally takes each pulse. */
	UPROPERTY()
	TArray<FVeyraContentId> Statuses;
};

/**
 * A Vanguard's companion (ADR-003's combat entity; ADR-034 §3), as Nix: its body, its stats and how they
 * grow with its owner, what it holds of its owner's, its basic attack and its behaviour.
 */
USTRUCT()
struct FVeyraCompanionTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	UPROPERTY()
	double CapsuleRadius = 0.0;

	UPROPERTY()
	double CapsuleHalfHeight = 0.0;

	/** Its base stats at its owner's first level. It has no resource. */
	UPROPERTY()
	FVeyraStatBlock Stats;

	/** What its base stats gain for each level its owner gains. */
	UPROPERTY()
	FVeyraStatBlock Growth;

	/** The share of its owner's Magic Power it holds as its own, at least 0: all it inherits (Combat Bible §32). */
	UPROPERTY()
	double OwnerMagicPowerShare = 0.0;

	UPROPERTY()
	FVeyraBasicAttackProfile BasicAttack;

	/** Following, how near its owner it keeps, edge to edge. */
	UPROPERTY()
	double FollowDistance = 0.0;

	/** How far from its owner it fights, follows a target or holds a point. */
	UPROPERTY()
	double LeashRange = 0.0;

	/** How far from itself, or from the point it holds, it looks for an enemy, edge to edge. */
	UPROPERTY()
	double AcquireRange = 0.0;

	/** How lately its owner must have fought an enemy for it to prefer that enemy, in seconds. */
	UPROPERTY()
	double OwnerTargetSeconds = 0.0;

	/** How long after it is killed it reforms beside its living owner, in seconds. */
	UPROPERTY()
	double ReformSeconds = 0.0;

	/** How often it and its keeper think, in seconds of world time. */
	UPROPERTY()
	double ThinkSeconds = 0.0;

	/** At most one: what it does now and then for the ally it escorts (ADR-035 §5). */
	UPROPERTY()
	TArray<FVeyraEscortTuning> Escort;

	/** Status IDs its basic attack's hit gives, as the Waterling's slow (ADR-035 §5). */
	UPROPERTY()
	TArray<FVeyraContentId> AttackStatuses;
};

USTRUCT()
struct FVeyraAbilitiesTuning
{
	GENERATED_BODY()

	/** The Abilities.json format this build reads (a schema version marker, not tuning). */
	static constexpr int32 SchemaVersion = 22;

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
	TMap<FVeyraContentId, FVeyraTetherAbilityTuning> Tether;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraAttachAbilityTuning> Attach;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraRideAbilityTuning> Ride;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraAmbushAbilityTuning> Ambush;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraStanceAbilityTuning> Stance;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraPlacementAbilityTuning> Placement;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraBlinkAbilityTuning> Blink;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraCommandAbilityTuning> Command;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraDismountAbilityTuning> Dismount;

	UPROPERTY()
	FVeyraFluxSpellsTuning FluxSpells;

	/** Companions, by ID (ADR-034 §3): no ability, so no archetype map. */
	UPROPERTY()
	TMap<FVeyraContentId, FVeyraCompanionTuning> Companions;
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

	/** Ability's resource cost at Rank, from whichever archetype map defines it; 0 for none. */
	VEYRAABILITIES_API double ResourceCost(const FVeyraAbilitiesTuning& Tuning, const FVeyraContentId& Ability, int32 Rank);

	/** Ability's cast tuning, from whichever archetype map defines it; null for none or a targeted-damage ability, which has its own. */
	VEYRAABILITIES_API const FVeyraCastTuning* FindCast(const FVeyraAbilitiesTuning& Tuning, const FVeyraContentId& Ability);

	/**
	 * Problems with Ability as the ability of a slot with RankCount ranks: each of its rank lists must
	 * hold one value, or exactly RankCount (ADR-008 §3).
	 */
	VEYRAABILITIES_API TArray<FString> ValidateRanks(const FVeyraAbilitiesTuning& Tuning, const FVeyraContentId& Ability, int32 RankCount);
}
