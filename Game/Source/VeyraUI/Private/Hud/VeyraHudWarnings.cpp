// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Hud/VeyraHudWarnings.h"

namespace VeyraHudWarnings
{
bool Update(FVeyraWarningState& State, bool bTrouble, double Now, double StartSeconds, double ClearSeconds)
{
	if (bTrouble)
	{
		State.CalmSince.Reset();
		if (!State.TroubleSince.IsSet())
		{
			State.TroubleSince = Now;
		}
		State.bShowing |= Now - State.TroubleSince.GetValue() >= StartSeconds;
	}
	else
	{
		State.TroubleSince.Reset();
		if (!State.CalmSince.IsSet())
		{
			State.CalmSince = Now;
		}
		State.bShowing &= Now - State.CalmSince.GetValue() < ClearSeconds;
	}
	return State.bShowing;
}

bool IsConnectionTroubled(double LossFraction, double RoundTripMs, double MaxLossFraction, double MaxRoundTripMs)
{
	return LossFraction > MaxLossFraction || RoundTripMs > MaxRoundTripMs;
}

bool IsPerformanceTroubled(bool bForeground, double FramesPerSecond, double CapFps, double UncappedReference, double Fraction)
{
	const double Expected = CapFps > 0.0 ? CapFps : UncappedReference;
	return bForeground && Expected > 0.0 && FramesPerSecond < Expected * Fraction;
}
}