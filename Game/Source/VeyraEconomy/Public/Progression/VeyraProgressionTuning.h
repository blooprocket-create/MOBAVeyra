// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

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

/** The Progression domain's tuning, bound from Game/Tuning/Progression.json (ADR-006 §6). */
USTRUCT()
struct FVeyraProgressionTuning
{
	GENERATED_BODY()

	/** The Progression.json format this build reads (a schema version marker, not tuning). */
	static constexpr int32 SchemaVersion = 1;

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
};
