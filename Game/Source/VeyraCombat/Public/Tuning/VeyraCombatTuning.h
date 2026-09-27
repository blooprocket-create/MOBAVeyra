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

/** The Combat domain's tuning, bound from Game/Tuning/Combat.json (ADR-006 §6). */
USTRUCT()
struct FVeyraCombatTuning
{
	GENERATED_BODY()

	/** The Combat.json format this build reads (a schema version marker, not tuning). */
	static constexpr int32 SchemaVersion = 2;

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
	FVeyraAttackSpeedTuning AttackSpeed;
};

/** The Combat domain's checks that a schema cannot express. */
namespace VeyraCombatTuningRules
{
	/** Problems with Tuning, each a JSON pointer and a message; empty when it is consistent. */
	VEYRACOMBAT_API TArray<FString> Validate(const FVeyraCombatTuning& Tuning);
}
