// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Progression/VeyraProgressionRules.h"

#include "Progression/VeyraProgressionTuning.h"

const TCHAR* LexToString(EVeyraRankRefusal Refusal)
{
	switch (Refusal)
	{
	case EVeyraRankRefusal::None:
		return TEXT("accepted");
	case EVeyraRankRefusal::NoSkillPoint:
		return TEXT("no unspent skill point");
	case EVeyraRankRefusal::MaxRank:
		return TEXT("already at its top rank");
	case EVeyraRankRefusal::LevelTooLow:
		return TEXT("the next rank opens at a higher level");
	case EVeyraRankRefusal::NotInitialized:
		return TEXT("the unit has not started progressing");
	}
	return TEXT("unknown");
}

namespace VeyraProgression
{
int32 ExperienceToNextLevel(int32 Level, const FVeyraProgressionTuning& Tuning)
{
	// ToNextLevel[0] takes level 1 to level 2.
	const int32 Index = Level - 1;
	return Level < Tuning.MaxLevel && Tuning.Experience.ToNextLevel.IsValidIndex(Index) ? Tuning.Experience.ToNextLevel[Index] : 0;
}

FExperienceState AddExperience(const FExperienceState& State, int32 Amount, const FVeyraProgressionTuning& Tuning, int32& LevelsGained)
{
	FExperienceState Result = State;
	LevelsGained = 0;
	int64 Pool = static_cast<int64>(Result.Experience) + FMath::Max(Amount, 0);
	while (Result.Level < Tuning.MaxLevel)
	{
		const int32 Needed = ExperienceToNextLevel(Result.Level, Tuning);
		if (Needed <= 0 || Pool < Needed)
		{
			break;
		}
		Pool -= Needed;
		++Result.Level;
		++LevelsGained;
	}
	// XP past the cap is discarded (§9).
	Result.Experience = Result.Level >= Tuning.MaxLevel ? 0 : static_cast<int32>(Pool);
	return Result;
}

int32 SkillPointsEarned(int32 Level, const FVeyraProgressionTuning& Tuning)
{
	return FMath::Clamp(Level, 0, Tuning.MaxLevel) * Tuning.SkillPointsPerLevel;
}

int32 MaxRank(EVeyraAbilitySlot Slot, const FVeyraProgressionTuning& Tuning)
{
	return VeyraAbilitySlots::IsUltimate(Slot) ? Tuning.UltimateMaxRank : Tuning.BasicAbilityMaxRank;
}

int32 MaxRankAtLevel(EVeyraAbilitySlot Slot, int32 Level, const FVeyraProgressionTuning& Tuning)
{
	if (!VeyraAbilitySlots::IsUltimate(Slot))
	{
		return Tuning.BasicAbilityMaxRank;
	}
	int32 Open = 0;
	for (const int32 OpensAt : Tuning.UltimateRankLevels)
	{
		Open += Level >= OpensAt ? 1 : 0;
	}
	return FMath::Min(Open, Tuning.UltimateMaxRank);
}

EVeyraRankRefusal CheckRankUp(EVeyraAbilitySlot Slot, int32 CurrentRank, int32 Level, int32 UnspentPoints, const FVeyraProgressionTuning& Tuning)
{
	if (CurrentRank >= MaxRank(Slot, Tuning))
	{
		return EVeyraRankRefusal::MaxRank;
	}
	if (CurrentRank >= MaxRankAtLevel(Slot, Level, Tuning))
	{
		return EVeyraRankRefusal::LevelTooLow;
	}
	if (UnspentPoints <= 0)
	{
		return EVeyraRankRefusal::NoSkillPoint;
	}
	return EVeyraRankRefusal::None;
}

TArray<FString> Validate(const FVeyraProgressionTuning& Tuning)
{
	TArray<FString> Problems;
	const TArray<int32>& Curve = Tuning.Experience.ToNextLevel;
	if (Curve.Num() != Tuning.MaxLevel - 1)
	{
		Problems.Add(FString::Printf(TEXT("/experience/toNextLevel: has %d entries; it needs one per level below maxLevel, %d"), Curve.Num(), Tuning.MaxLevel - 1));
	}
	for (int32 Index = 0; Index < Curve.Num(); ++Index)
	{
		if (Curve[Index] <= 0)
		{
			Problems.Add(FString::Printf(TEXT("/experience/toNextLevel/%d: must be above 0"), Index));
		}
	}
	if (Tuning.UltimateRankLevels.Num() != Tuning.UltimateMaxRank)
	{
		Problems.Add(FString::Printf(TEXT("/ultimateRankLevels: has %d entries; it needs one per ultimate rank, %d"), Tuning.UltimateRankLevels.Num(), Tuning.UltimateMaxRank));
	}
	int32 Previous = 0;
	for (int32 Index = 0; Index < Tuning.UltimateRankLevels.Num(); ++Index)
	{
		const int32 OpensAt = Tuning.UltimateRankLevels[Index];
		if (OpensAt <= Previous || OpensAt > Tuning.MaxLevel)
		{
			Problems.Add(FString::Printf(TEXT("/ultimateRankLevels/%d: must be above the previous entry and at most maxLevel"), Index));
		}
		Previous = OpensAt;
	}
	return Problems;
}
}
