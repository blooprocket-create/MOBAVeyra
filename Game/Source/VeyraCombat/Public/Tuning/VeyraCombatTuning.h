// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Tuning/VeyraTuningProvenance.h"
#include "UObject/ObjectMacros.h"

#include "VeyraCombatTuning.generated.h"

/** Armor and Magic Resistance mitigation (Combat Bible §3). */
USTRUCT()
struct FVeyraResistanceTuning
{
	GENERATED_BODY()

	/**
	 * K in "damage taken = raw × K / (K + resistance)". Bound from Game/Tuning/Combat.json, whose
	 * schema requires it to be greater than 0; 0 here only means "not loaded".
	 */
	UPROPERTY()
	double MitigationConstant = 0.0;
};

/** Server-side checks of whether a target is valid and in range (Combat Bible §30, §40). */
USTRUCT()
struct FVeyraTargetingTuning
{
	GENERATED_BODY()

	/**
	 * Extra range, in units, the server allows on top of a cast range, so a target that just left
	 * range on the caster's screen is still accepted (Combat Bible §30).
	 */
	UPROPERTY()
	double ServerRangeTolerance = 0.0;
};

/** Resource regeneration over time (Combat Bible §28). */
USTRUCT()
struct FVeyraRegenerationTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/**
	 * How often regeneration is applied, in seconds; each tick restores the per-second rate times this.
	 * The schema requires it to be above 0; 0 here only means "not loaded".
	 */
	UPROPERTY()
	double TickSeconds = 0.0;
};

/** One Movement Speed soft cap (Combat Bible §23). */
USTRUCT()
struct FVeyraSpeedSoftCap
{
	GENERATED_BODY()

	/** The speed, in units per second, where the cap begins. */
	UPROPERTY()
	double From = 0.0;

	/** The fraction of each unit of speed above From that is kept, until the next cap. */
	UPROPERTY()
	double Retained = 0.0;
};

/** Effective Movement Speed (Combat Bible §23, §39). */
USTRUCT()
struct FVeyraMovementTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** In strictly rising order of From (VeyraCombatTuningRules::Validate). */
	UPROPERTY()
	TArray<FVeyraSpeedSoftCap> SoftCaps;

	/** The lowest speed slowing can bring a unit to, unless its base speed is lower. */
	UPROPERTY()
	double SlowFloor = 0.0;
};

/** Displacement and dashes (Combat Bible §9, ADR-009 §2). */
USTRUCT()
struct FVeyraForcedMovementTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** How far a forced movement's end may be moved to reach walkable ground, in units and in each direction. */
	UPROPERTY()
	double NavigationExtent = 0.0;
};

/**
 * Motions (ADR-079): what the server tells every machine of each unit's movement, how every machine draws a body from it,
 * and the clock both read it by. Network presentation, not balance: none of it changes where the server moves a body.
 */
USTRUCT()
struct FVeyraMotionTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** How far ahead a walking body's corners are told, in seconds of its walking (§2). */
	UPROPERTY()
	double HorizonSeconds = 0.0;

	/** A walk is told afresh once less than this is left of what was told, in seconds of its walking (§2). */
	UPROPERTY()
	double RefreshSeconds = 0.0;

	/** A body this far from where its told motion has it, in units, is told afresh (§2). */
	UPROPERTY()
	double DriftDistance = 0.0;

	/** A body whose speed changes by more than this share of its told speed is told afresh (§2). */
	UPROPERTY()
	double SpeedChangeShare = 0.0;

	/** A body moving slower than this share of its speed, though its path runs on, is told standing (§2). */
	UPROPERTY()
	double StandingSpeedShare = 0.0;

	/** A standing body facing this far from its told yaw, in degrees, is told afresh (§2). */
	UPROPERTY()
	double FacingDegrees = 0.0;

	/** A body found this far from where its told motion has it, in units, was set down there at once, and is drawn so (§2). */
	UPROPERTY()
	double SetDownDistance = 0.0;

	/** A drawn body eases from where it was drawn onto a new motion over this many seconds (§3). */
	UPROPERTY()
	double CorrectionSeconds = 0.0;

	/** A drawn body this far from a new motion, in units, is drawn on it at once (§3). */
	UPROPERTY()
	double CorrectionSnapDistance = 0.0;

	/** Bodies other than the player's own are drawn this many seconds behind the server's clock now (§3). */
	UPROPERTY()
	double RemoteDelaySeconds = 0.0;

	/** The most the player's own body is drawn ahead of its server motion, in seconds (§5). */
	UPROPERTY()
	double OwnLeadMaxSeconds = 0.0;

	/** How long the player's own body walks an order the server has not yet answered before it waits, in seconds (§5). */
	UPROPERTY()
	double OwnPredictionSeconds = 0.0;

	/** A client times an exchange with the server's clock this often, in seconds (§4). */
	UPROPERTY()
	double ClockExchangeSeconds = 0.0;

	/** How many of its latest exchanges a client keeps, trusting the quickest (§4). */
	UPROPERTY()
	int32 ClockExchangesKept = 0;

	/** How fast a client's clock estimate moves toward a new one, in seconds a second (§4). */
	UPROPERTY()
	double ClockSlewRate = 0.0;

	/** A client's clock estimate this far off a new one, in seconds, is set at once (§4). */
	UPROPERTY()
	double ClockSnapSeconds = 0.0;
};

/** The playable ground (ADR-040 §4). */
USTRUCT()
struct FVeyraGroundTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** How far above and below a body or a point its ground is looked for, in units. */
	UPROPERTY()
	double SearchHeight = 0.0;
};

/** Crowd control (Combat Bible §8). */
USTRUCT()
struct FVeyraCrowdControlTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** The shortest duration Tenacity can shorten a crowd-control effect to, in seconds. */
	UPROPERTY()
	double TenacityFloorSeconds = 0.0;
};

/** Vanguard Combat State (Combat Bible §28). */
USTRUCT()
struct FVeyraCombatStateTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** How long after its last Vanguard combat a unit leaves Combat State, in seconds. */
	UPROPERTY()
	double OutOfCombatSeconds = 0.0;
};

/** Kill and assist credit (Combat Bible §18). */
USTRUCT()
struct FVeyraAttributionTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** How long a contribution to a Vanguard's death counts toward an assist, in seconds. */
	UPROPERTY()
	double AssistWindowSeconds = 0.0;
};

/** Kill credit for a death the environment finishes (Combat Bible §18). */
USTRUCT()
struct FVeyraKillCreditTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** How recently an enemy Vanguard must have contributed to be credited with such a kill, in seconds. */
	UPROPERTY()
	double WindowSeconds = 0.0;
};

/** Basic attacks against structures (Combat Bible §33). */
USTRUCT()
struct FVeyraStructureCombatTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** Structure Effectiveness: the fraction of a basic attack's secondary riders a structure takes. */
	UPROPERTY()
	double Effectiveness = 0.0;
};

/** Critical strikes (Combat Bible §5; ADR-023 §1). */
USTRUCT()
struct FVeyraCritTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** What a crit multiplies the attack's base damage by before bonuses: Normal Crit Damage. */
	UPROPERTY()
	double Damage = 0.0;

	/** The most effective Crit Chance, as a fraction. */
	UPROPERTY()
	double ChanceCap = 0.0;

	/** The Crit Damage each 1 of Crit Chance above the cap adds. */
	UPROPERTY()
	double OverflowDamagePerChance = 0.0;
};

/** An outcome bag's size (ADR-023 §10): how many draws one shuffled bag holds before it refills. */
USTRUCT()
struct FVeyraOutcomeBagTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** Smaller is steadier; 1 is an ordinary independent roll. */
	UPROPERTY()
	int32 Draws = 0;
};

/** Attack Speed limits and overflow (Combat Bible §22, §39). */
USTRUCT()
struct FVeyraAttackSpeedTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** The normal maximum in attacks per second, and the permanent reference overflow is measured from. */
	UPROPERTY()
	double Cap = 0.0;

	/** The ordinary minimum in attacks per second; below the cap (VeyraCombatTuningRules::Validate). */
	UPROPERTY()
	double Minimum = 0.0;

	/** S in overflow damage % = S × overflow / (C + overflow), overflow being a percentage of the cap. */
	UPROPERTY()
	double OverflowDamageScalePercent = 0.0;

	/** C in the same formula. */
	UPROPERTY()
	double OverflowCurveConstant = 0.0;
};

/**
 * When a unit counts as moving toward an enemy Vanguard, for bonuses that hold only then (Combat
 * Bible §23; ADR-008 §9): a living enemy Vanguard it can acquire (VeyraTargeting::CanAcquire) is
 * within Range, edge to edge, and lies within MaxAngleDegrees of the way it is moving.
 */
USTRUCT()
struct FVeyraPursuitTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	UPROPERTY()
	double Range = 0.0;

	/** Either side of the unit's movement direction, in degrees; above 0 and below 90. */
	UPROPERTY()
	double MaxAngleDegrees = 0.0;
};

/** How the server keeps its tethers (Combat Bible §43; ADR-018). */
USTRUCT()
struct FVeyraTetherCombatTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** How often each tether's range, time and units are judged, in seconds; above 0. */
	UPROPERTY()
	double CheckSeconds = 0.0;
};

/** The Combat domain's tuning, bound from Game/Tuning/Combat.json (ADR-006 §6). */
USTRUCT()
struct FVeyraCombatTuning
{
	GENERATED_BODY()

	/** The Combat.json format this build reads (a schema version marker, not tuning). */
	static constexpr int32 SchemaVersion = 7;

	UPROPERTY()
	FVeyraResistanceTuning Resistance;

	UPROPERTY()
	FVeyraTargetingTuning Targeting;

	UPROPERTY()
	FVeyraRegenerationTuning Regeneration;

	UPROPERTY()
	FVeyraMovementTuning Movement;

	UPROPERTY()
	FVeyraForcedMovementTuning ForcedMovement;

	UPROPERTY()
	FVeyraMotionTuning Motion;

	UPROPERTY()
	FVeyraGroundTuning Ground;

	UPROPERTY()
	FVeyraCrowdControlTuning CrowdControl;

	UPROPERTY()
	FVeyraCombatStateTuning CombatState;

	UPROPERTY()
	FVeyraAttributionTuning Attribution;

	UPROPERTY()
	FVeyraKillCreditTuning KillCredit;

	UPROPERTY()
	FVeyraStructureCombatTuning Structures;

	UPROPERTY()
	FVeyraAttackSpeedTuning AttackSpeed;

	UPROPERTY()
	FVeyraCritTuning Crit;

	/** The bag every crit channel draws from (ADR-023 §10). */
	UPROPERTY()
	FVeyraOutcomeBagTuning CritBag;

	UPROPERTY()
	FVeyraPursuitTuning Pursuit;

	UPROPERTY()
	FVeyraTetherCombatTuning Tethers;
};

/** The Combat domain's checks that a schema cannot express. */
namespace VeyraCombatTuningRules
{
	/** Problems with Tuning, each a JSON pointer and a message; empty when it is consistent. */
	VEYRACOMBAT_API TArray<FString> Validate(const FVeyraCombatTuning& Tuning);
}
