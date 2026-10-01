// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Tuning/VeyraTuningProvenance.h"
#include "UObject/ObjectMacros.h"

#include "VeyraProgressionTuning.generated.h"

/** The XP a unit needs to reach each next level (Economy & Progression Bible §9). */
USTRUCT()
struct FVeyraExperienceTuning
{
	GENERATED_BODY()

	/** Canon gives no numbers for the curve, so it is drafted and marked (ADR-008 §7). */
	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/**
	 * XP from level N to N + 1, for N = 1 to maxLevel − 1: one entry per level below the cap. The
	 * curve is shared by every Vanguard.
	 */
	UPROPERTY()
	TArray<int32> ToNextLevel;
};

/** How a rank shape's R takes ranks (ADR-031 §2). */
UENUM()
enum class EVeyraUltimateRanks : uint8
{
	/** As standard: a point a rank, each rank opening at its level. */
	Ranked,
	/** Learnt from the start at rank 1: it takes no point and never ranks. */
	Innate,
};

/**
 * A documented exception to how a kit takes ranks (Economy & Progression Bible §1; ADR-031 §2). It
 * spends the standard total of skill points.
 */
USTRUCT()
struct FVeyraRankShapeTuning
{
	GENERATED_BODY()

	/** The top rank of Q, W and E. */
	UPROPERTY()
	int32 BasicAbilityMaxRank = 0;

	UPROPERTY()
	EVeyraUltimateRanks Ultimate = EVeyraUltimateRanks::Ranked;
};

/** The Progression domain's tuning, bound from Game/Tuning/Progression.json (ADR-006 §6). */
USTRUCT()
struct FVeyraProgressionTuning
{
	GENERATED_BODY()

	/** The Progression.json format this build reads (a schema version marker, not tuning). */
	static constexpr int32 SchemaVersion = 2;

	/** The level cap (Economy & Progression §9: 18). */
	UPROPERTY()
	int32 MaxLevel = 0;

	/** Skill points gained at level 1 and at each level after it (§1, §9: one). */
	UPROPERTY()
	int32 SkillPointsPerLevel = 0;

	/** The top rank of Q, W and E (§1: 5). */
	UPROPERTY()
	int32 BasicAbilityMaxRank = 0;

	/** The top rank of R (§1: 3). */
	UPROPERTY()
	int32 UltimateMaxRank = 0;

	/** The level at which each ultimate rank opens, one entry per rank (§1: 6, 11, 16). */
	UPROPERTY()
	TArray<int32> UltimateRankLevels;

	UPROPERTY()
	FVeyraExperienceTuning Experience;

	/** Documented exceptions to the standard ranks, by ID; a Vanguard record names at most one (ADR-031 §2). */
	UPROPERTY()
	TMap<FVeyraContentId, FVeyraRankShapeTuning> RankShapes;
};
