// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Progression/VeyraProgressionTypes.h"
#include "Slots/VeyraAbilitySlot.h"

struct FVeyraProgressionTuning;

/**
 * In-match progression rules (Economy & Progression Bible §1, §9), as pure functions of the tuning,
 * so they are tested without a world. UVeyraProgressionComponent applies them.
 */
namespace VeyraProgression
{
	/** A unit's place on the XP curve. */
	struct FExperienceState
	{
		int32 Level = 1;
		/** XP gained towards the next level; always 0 at the cap. */
		int32 Experience = 0;
	};

	/** XP needed to go from Level to Level + 1; 0 at the cap. */
	VEYRAECONOMY_API int32 ExperienceToNextLevel(int32 Level, const FVeyraProgressionTuning& Tuning);

	/**
	 * Adds Amount XP (at least 0). One reward may grant several levels, and XP past the cap is
	 * discarded (§9). Returns the new state; LevelsGained tells how many levels it crossed.
	 */
	VEYRAECONOMY_API FExperienceState AddExperience(const FExperienceState& State, int32 Amount, const FVeyraProgressionTuning& Tuning, int32& LevelsGained);

	/** Skill points a unit has earned by Level in total (§9: one per level, from level 1). */
	VEYRAECONOMY_API int32 SkillPointsEarned(int32 Level, const FVeyraProgressionTuning& Tuning);

	/** The top rank Slot can ever reach (§1: 5 for Q, W and E; 3 for R). */
	VEYRAECONOMY_API int32 MaxRank(EVeyraAbilitySlot Slot, const FVeyraProgressionTuning& Tuning);

	/** The top rank Slot may hold at Level: Q, W and E have no level gate; R opens at set levels (§1). */
	VEYRAECONOMY_API int32 MaxRankAtLevel(EVeyraAbilitySlot Slot, int32 Level, const FVeyraProgressionTuning& Tuning);

	/** Whether a unit at Level with UnspentPoints may raise Slot from CurrentRank. Points cannot be taken back (§9). */
	VEYRAECONOMY_API EVeyraRankRefusal CheckRankUp(EVeyraAbilitySlot Slot, int32 CurrentRank, int32 Level, int32 UnspentPoints,
		const FVeyraProgressionTuning& Tuning);

	/**
	 * What the schema cannot check: the curve has one entry per level below the cap, each above 0; each
	 * ultimate rank has one opening level, strictly rising and within the cap. Returns every problem.
	 */
	VEYRAECONOMY_API TArray<FString> Validate(const FVeyraProgressionTuning& Tuning);
}
