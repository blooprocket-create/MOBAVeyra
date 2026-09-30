// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Buyback/VeyraBuybackRules.h"

#include "Rewards/VeyraEconomyTuning.h"

const TCHAR* LexToString(EVeyraBuybackRefusal Refusal)
{
	switch (Refusal)
	{
	case EVeyraBuybackRefusal::None:
		return TEXT("none");
	case EVeyraBuybackRefusal::Unavailable:
		return TEXT("unavailable");
	case EVeyraBuybackRefusal::TooEarly:
		return TEXT("too early");
	case EVeyraBuybackRefusal::Alive:
		return TEXT("alive");
	case EVeyraBuybackRefusal::CoolingDown:
		return TEXT("cooling down");
	case EVeyraBuybackRefusal::NotEnoughGold:
		return TEXT("not enough Gold");
	}
	return TEXT("unknown");
}

namespace VeyraBuyback
{
double Cost(double MatchSeconds, int32 Purchases, const FVeyraBuybackTuning& Tuning)
{
	constexpr double SecondsPerMinute = 60.0;
	const double WholeMinutes = FMath::FloorToDouble(FMath::Max(0.0, MatchSeconds - Tuning.AvailableFromSeconds) / SecondsPerMinute);
	return Tuning.BaseCost + Tuning.CostPerMinute * WholeMinutes + Tuning.CostPerPurchase * FMath::Max(0, Purchases);
}

FVeyraBuybackQuote Quote(double MatchSeconds, double Now, bool bDead, double Gold, int32 Purchases, double ReadyAt, const FVeyraBuybackTuning& Tuning)
{
	FVeyraBuybackQuote Out;
	Out.Cost = Cost(MatchSeconds, Purchases, Tuning);
	if (!bDead)
	{
		Out.Refusal = EVeyraBuybackRefusal::Alive;
	}
	else if (MatchSeconds < Tuning.AvailableFromSeconds)
	{
		Out.Refusal = EVeyraBuybackRefusal::TooEarly;
	}
	else if (Now < ReadyAt)
	{
		Out.Refusal = EVeyraBuybackRefusal::CoolingDown;
	}
	else if (Gold < Out.Cost)
	{
		Out.Refusal = EVeyraBuybackRefusal::NotEnoughGold;
	}
	return Out;
}
}
