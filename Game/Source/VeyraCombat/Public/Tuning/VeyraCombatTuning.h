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

/** Critical strikes (Combat Bible §5; ADR-022 §1). */
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

/** An outcome bag's size (ADR-022 §10): how many draws one shuffled bag holds before it refills. */
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
	static constexpr int32 SchemaVersion = 6;

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

	/** The bag every crit channel draws from (ADR-022 §10). */
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
