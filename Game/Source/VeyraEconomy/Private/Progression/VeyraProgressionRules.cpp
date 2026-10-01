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

FExperienceState AddExperience(const FExperienceState& State, double Amount, const FVeyraProgressionTuning& Tuning, int32& LevelsGained)
{
	FExperienceState Result = State;
	LevelsGained = 0;
	// Full fractional precision (§1): a shared reward's share is kept whole.
	double Pool = Result.Experience + (FMath::IsFinite(Amount) ? FMath::Max(Amount, 0.0) : 0.0);
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
	Result.Experience = Result.Level >= Tuning.MaxLevel ? 0.0 : Pool;
	return Result;
}

int32 SkillPointsEarned(int32 Level, const FVeyraProgressionTuning& Tuning)
{
	return FMath::Clamp(Level, 0, Tuning.MaxLevel) * Tuning.SkillPointsPerLevel;
}

FVeyraRankShape StandardShape(const FVeyraProgressionTuning& Tuning)
{
	FVeyraRankShape Shape;
	Shape.BasicAbilityMaxRank = Tuning.BasicAbilityMaxRank;
	Shape.bUltimateInnate = false;
	return Shape;
}

TOptional<FVeyraRankShape> FindShape(const FVeyraProgressionTuning& Tuning, const FVeyraContentId& Id)
{
	const FVeyraRankShapeTuning* Found = Tuning.RankShapes.Find(Id);
	if (!Found)
	{
		return TOptional<FVeyraRankShape>();
	}
	FVeyraRankShape Shape;
	Shape.BasicAbilityMaxRank = Found->BasicAbilityMaxRank;
	Shape.bUltimateInnate = Found->Ultimate == EVeyraUltimateRanks::Innate;
	return Shape;
}

int32 MaxRank(EVeyraAbilitySlot Slot, const FVeyraProgressionTuning& Tuning, const FVeyraRankShape& Shape)
{
	if (!VeyraAbilitySlots::IsUltimate(Slot))
	{
		return Shape.BasicAbilityMaxRank;
	}
	// An innate R holds the one rank it starts with (ADR-031 §2).
	return Shape.bUltimateInnate ? 1 : Tuning.UltimateMaxRank;
}

int32 MaxRank(EVeyraAbilitySlot Slot, const FVeyraProgressionTuning& Tuning)
{
	return MaxRank(Slot, Tuning, StandardShape(Tuning));
}

int32 MaxRankAtLevel(EVeyraAbilitySlot Slot, int32 Level, const FVeyraProgressionTuning& Tuning, const FVeyraRankShape& Shape)
{
	if (!VeyraAbilitySlots::IsUltimate(Slot))
	{
		return Shape.BasicAbilityMaxRank;
	}
	if (Shape.bUltimateInnate)
	{
		return 1;
	}
	int32 Open = 0;
	for (const int32 OpensAt : Tuning.UltimateRankLevels)
	{
		Open += Level >= OpensAt ? 1 : 0;
	}
	return FMath::Min(Open, Tuning.UltimateMaxRank);
}

int32 MaxRankAtLevel(EVeyraAbilitySlot Slot, int32 Level, const FVeyraProgressionTuning& Tuning)
{
	return MaxRankAtLevel(Slot, Level, Tuning, StandardShape(Tuning));
}

int32 StartingRank(EVeyraAbilitySlot Slot, const FVeyraRankShape& Shape)
{
	return VeyraAbilitySlots::IsUltimate(Slot) && Shape.bUltimateInnate ? 1 : 0;
}

EVeyraRankRefusal CheckRankUp(EVeyraAbilitySlot Slot, int32 CurrentRank, int32 Level, int32 UnspentPoints, const FVeyraProgressionTuning& Tuning,
	const FVeyraRankShape& Shape)
{
	if (CurrentRank >= MaxRank(Slot, Tuning, Shape))
	{
		return EVeyraRankRefusal::MaxRank;
	}
	if (CurrentRank >= MaxRankAtLevel(Slot, Level, Tuning, Shape))
	{
		return EVeyraRankRefusal::LevelTooLow;
	}
	if (UnspentPoints <= 0)
	{
		return EVeyraRankRefusal::NoSkillPoint;
	}
	return EVeyraRankRefusal::None;
}

EVeyraRankRefusal CheckRankUp(EVeyraAbilitySlot Slot, int32 CurrentRank, int32 Level, int32 UnspentPoints, const FVeyraProgressionTuning& Tuning)
{
	return CheckRankUp(Slot, CurrentRank, Level, UnspentPoints, Tuning, StandardShape(Tuning));
}

TArray<int32> RankCounts(const FVeyraProgressionTuning& Tuning)
{
	TArray<int32> Counts = { Tuning.BasicAbilityMaxRank, Tuning.UltimateMaxRank };
	for (const TPair<FVeyraContentId, FVeyraRankShapeTuning>& Shape : Tuning.RankShapes)
	{
		Counts.AddUnique(Shape.Value.BasicAbilityMaxRank);
		if (Shape.Value.Ultimate == EVeyraUltimateRanks::Innate)
		{
			Counts.AddUnique(1);
		}
	}
	return Counts;
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
	// A documented exception moves points between slots; it never changes how many a kit spends (ADR-031 §2).
	const int32 StandardTotal = 3 * Tuning.BasicAbilityMaxRank + Tuning.UltimateMaxRank;
	for (const TPair<FVeyraContentId, FVeyraRankShapeTuning>& Shape : Tuning.RankShapes)
	{
		const int32 Total = 3 * Shape.Value.BasicAbilityMaxRank + (Shape.Value.Ultimate == EVeyraUltimateRanks::Innate ? 0 : Tuning.UltimateMaxRank);
		if (Total != StandardTotal)
		{
			Problems.Add(FString::Printf(TEXT("/rankShapes/%s: spends %d skill points; a rank shape spends the standard %d"), *Shape.Key.ToString(), Total,
				StandardTotal));
		}
	}
	return Problems;
}
}
