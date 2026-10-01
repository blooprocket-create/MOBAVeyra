// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "UObject/ObjectMacros.h"

#include "VeyraProgressionTypes.generated.h"

/**
 * What a Vanguard's base stats gain at each level after the first (Economy & Progression Bible §9:
 * Vanguard-specific, data-driven growth). Vanguard definitions bind it from tuning and hand it to
 * Progression, which applies it through VeyraCombat::GrowBaseStats.
 */
USTRUCT()
struct VEYRAECONOMY_API FVeyraStatGrowth
{
	GENERATED_BODY()

	UPROPERTY()
	double MaxHealth = 0.0;

	UPROPERTY()
	double HealthRegen = 0.0;

	UPROPERTY()
	double MaxResource = 0.0;

	UPROPERTY()
	double ResourceRegen = 0.0;

	UPROPERTY()
	double Armor = 0.0;

	UPROPERTY()
	double MagicResist = 0.0;

	UPROPERTY()
	double PhysicalPower = 0.0;

	UPROPERTY()
	double MagicPower = 0.0;

	/** Each level adds this fraction of the level-1 Attack Speed. */
	UPROPERTY()
	double AttackSpeedFraction = 0.0;
};

/**
 * How a unit's kit takes ranks (Economy & Progression Bible §1; ADR-031 §2): the standard shape, or a
 * documented exception, as Angeru's Q, W and E to 6 with an R learnt from the start.
 */
USTRUCT()
struct VEYRAECONOMY_API FVeyraRankShape
{
	GENERATED_BODY()

	/** The top rank of Q, W and E. */
	UPROPERTY()
	int32 BasicAbilityMaxRank = 0;

	/** Whether R is learnt from the start, at rank 1, and never ranks. */
	UPROPERTY()
	bool bUltimateInnate = false;
};

/** Why a rank-up was refused. None means it was accepted. */
UENUM()
enum class EVeyraRankRefusal : uint8
{
	None,
	/** No unspent skill point. */
	NoSkillPoint,
	/** The ability is already at its top rank. */
	MaxRank,
	/** The next ultimate rank opens at a higher level (Economy & Progression §1). */
	LevelTooLow,
	/** The unit has not started progressing. */
	NotInitialized,
};

VEYRAECONOMY_API const TCHAR* LexToString(EVeyraRankRefusal Refusal);
