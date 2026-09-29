// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Rewards/VeyraRewardRules.h"

#include "Rewards/VeyraEconomyTuning.h"

namespace VeyraRewards
{
TArray<FString> Validate(const FVeyraEconomyTuning& Tuning)
{
	TArray<FString> Problems;
	for (const TPair<FVeyraContentId, double>& Gold : Tuning.Gold.Fluxborn)
	{
		if (!Tuning.Experience.Fluxborn.Contains(Gold.Key))
		{
			Problems.Add(FString::Printf(TEXT("/experience/fluxborn: %s has Gold but no XP"), *Gold.Key.ToString()));
		}
	}
	for (const TPair<FVeyraContentId, double>& Experience : Tuning.Experience.Fluxborn)
	{
		if (!Tuning.Gold.Fluxborn.Contains(Experience.Key))
		{
			Problems.Add(FString::Printf(TEXT("/gold/fluxborn: %s has XP but no Gold"), *Experience.Key.ToString()));
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
}
