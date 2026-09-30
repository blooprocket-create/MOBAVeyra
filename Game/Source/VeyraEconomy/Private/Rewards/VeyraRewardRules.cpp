// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Rewards/VeyraRewardRules.h"

#include "Rewards/VeyraEconomyTuning.h"

namespace VeyraRewards
{
TArray<FString> Validate(const FVeyraEconomyTuning& Tuning)
{
	TArray<FString> Problems;
	// Every Fluxborn, and every species of wildlife, that pays Gold gives XP, and the reverse.
	const auto CheckPaired = [&Problems](const TMap<FVeyraContentId, double>& Gold, const TMap<FVeyraContentId, double>& Experience, const TCHAR* Field) {
		for (const TPair<FVeyraContentId, double>& Entry : Gold)
		{
			if (!Experience.Contains(Entry.Key))
			{
				Problems.Add(FString::Printf(TEXT("/experience/%s: %s has Gold but no XP"), Field, *Entry.Key.ToString()));
			}
		}
		for (const TPair<FVeyraContentId, double>& Entry : Experience)
		{
			if (!Gold.Contains(Entry.Key))
			{
				Problems.Add(FString::Printf(TEXT("/gold/%s: %s has XP but no Gold"), Field, *Entry.Key.ToString()));
			}
		}
	};
	CheckPaired(Tuning.Gold.Fluxborn, Tuning.Experience.Fluxborn, TEXT("fluxborn"));
	CheckPaired(Tuning.Gold.Wildlife, Tuning.Experience.Wildlife, TEXT("wildlife"));
	for (int32 Index = 1; Index < Tuning.Bounty.ByStreak.Num(); ++Index)
	{
		if (Tuning.Bounty.ByStreak[Index] < Tuning.Bounty.ByStreak[Index - 1])
		{
			Problems.Add(FString::Printf(TEXT("/bounty/byStreak/%d: a longer streak's bounty is less than a shorter one's"), Index));
		}
	}
	const TArray<double>& Steps = Tuning.KillGold.DevaluationSteps;
	if (!Steps.IsEmpty() && Steps[0] != 1.0)
	{
		Problems.Add(TEXT("/killGold/devaluationSteps/0: kill Gold starts whole, at 1"));
	}
	for (int32 Index = 1; Index < Steps.Num(); ++Index)
	{
		if (Steps[Index] > Steps[Index - 1])
		{
			Problems.Add(FString::Printf(TEXT("/killGold/devaluationSteps/%d: a longer death streak's step is worth more than a shorter one's"), Index));
		}
	}
	return Problems;
}

double FluxBonusMultiplier(double ActiveFlux, const FVeyraFluxRewardBonusTuning& Bonus)
{
	if (!(Bonus.FluxPerStep > 0.0) || !(ActiveFlux > 0.0))
	{
		return 1.0;
	}
	const double Steps = FMath::FloorToDouble(ActiveFlux / Bonus.FluxPerStep);
	return 1.0 + FMath::Min(Bonus.MaxBonus, Steps * Bonus.BonusPerStep);
}

double FarmXpShare(double BaseXp, int32 Eligible, double SharedPoolFraction)
{
	if (Eligible <= 0)
	{
		return 0.0;
	}
	return Eligible == 1 ? BaseXp : BaseXp * SharedPoolFraction / Eligible;
}

double KillExperience(int32 VictimLevel, const FVeyraExperienceRewardTuning& Experience)
{
	return Experience.KillBase + Experience.KillPerLevel * FMath::Max(0, VictimLevel - 1);
}

double KillExperiencePool(double Base, int32 Participants, bool bVictimOutlevelsKiller, const FVeyraExperienceRewardTuning& Experience)
{
	const double Pool = Base * (1.0 + Experience.ParticipantBonus * FMath::Max(0, Participants - 1));
	return bVictimOutlevelsKiller ? Pool * Experience.HigherLevelVictimMultiplier : Pool;
}

double AssistShare(double KillGold, int32 Assisters, double AssistPoolFraction)
{
	return Assisters > 0 ? KillGold * AssistPoolFraction / Assisters : 0.0;
}

double Bounty(int32 KillStreak, const FVeyraBountyTuning& Bounty)
{
	const TArray<double>& ByStreak = Bounty.ByStreak;
	return ByStreak.IsEmpty() ? 0.0 : ByStreak[FMath::Clamp(KillStreak, 0, ByStreak.Num() - 1)];
}

double DevaluedKillGold(double BaseKillGold, int32 DeathStreak, const FVeyraKillGoldTuning& KillGold)
{
	const TArray<double>& Steps = KillGold.DevaluationSteps;
	return Steps.IsEmpty() ? BaseKillGold : BaseKillGold * Steps[FMath::Clamp(DeathStreak, 0, Steps.Num() - 1)];
}

int32 DeathStreakAfterDeath(int32 DeathStreak, const FVeyraKillGoldTuning& KillGold)
{
	return FMath::Clamp(DeathStreak + 1, 0, FMath::Max(0, KillGold.DevaluationSteps.Num() - 1));
}

int32 DeathStreakAfterTakedown(int32 DeathStreak)
{
	return FMath::Max(0, DeathStreak - 1);
}
}
