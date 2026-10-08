// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Hud/VeyraLastHit.h"

#include "Math/UnrealMathUtility.h"

void FVeyraHealthLoss::Sample(double At, double Health, double KeepSeconds)
{
	const double Fell = LastHealth >= 0.0 ? LastHealth - Health : 0.0;
	LastHealth = Health;
	Note(At, Fell, KeepSeconds);
}

void FVeyraHealthLoss::Note(double At, double Amount, double KeepSeconds)
{
	Losses.RemoveAll([At, KeepSeconds](const FLoss& Loss) { return Loss.At < At - KeepSeconds; });
	if (Amount > 0.0)
	{
		Losses.Add(FLoss{ At, Amount });
	}
}

double FVeyraHealthLoss::PerSecond(double Now, double WindowSeconds) const
{
	if (!(WindowSeconds > 0.0))
	{
		return 0.0;
	}
	double Lost = 0.0;
	for (const FLoss& Loss : Losses)
	{
		if (Loss.At > Now - WindowSeconds && Loss.At <= Now)
		{
			Lost += Loss.Amount;
		}
	}
	return Lost / WindowSeconds;
}

EVeyraLastHitStage VeyraLastHit::StageOf(double Health, double Hit, double LossPerSecond, double LeadSeconds)
{
	if (!(Hit > 0.0))
	{
		return EVeyraLastHitStage::None;
	}
	if (Health <= Hit)
	{
		return EVeyraLastHitStage::Now;
	}
	const double Falls = FMath::Max(0.0, LossPerSecond) * FMath::Max(0.0, LeadSeconds);
	return Health <= Hit + Falls ? EVeyraLastHitStage::Ready : EVeyraLastHitStage::None;
}

double VeyraLastHit::LeadSecondsOf(double WindupSeconds, double Distance, double ProjectileSpeed)
{
	const double Flight = ProjectileSpeed > 0.0 ? FMath::Max(0.0, Distance) / ProjectileSpeed : 0.0;
	return FMath::Max(0.0, WindupSeconds) + Flight;
}
