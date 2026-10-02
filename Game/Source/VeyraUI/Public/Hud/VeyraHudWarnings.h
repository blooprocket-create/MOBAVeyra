// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Misc/Optional.h"

/**
 * One HUD warning over time (Settings Bible §3.6, Proposal 110; ADR-055 §5): it starts once its trouble has held for a
 * while, and clears once calm has held for a while, so a moment's hitch neither raises nor drops it.
 */
struct FVeyraWarningState
{
	bool bShowing = false;
	/** Since when the trouble or the calm has held without a break. */
	TOptional<double> TroubleSince;
	TOptional<double> CalmSince;
};

/** When the HUD's connection and frame-rate warnings show, apart from the engine. */
namespace VeyraHudWarnings
{
	/** Whether the warning shows at Now, its trouble holding now or not; it starts after StartSeconds of trouble and clears after ClearSeconds of calm. */
	VEYRAUI_API bool Update(FVeyraWarningState& State, bool bTrouble, double Now, double StartSeconds, double ClearSeconds);

	/** Whether a connection is in trouble: losing more than MaxLossFraction of its packets, or a round trip above MaxRoundTripMs. */
	VEYRAUI_API bool IsConnectionTroubled(double LossFraction, double RoundTripMs, double MaxLossFraction, double MaxRoundTripMs);

	/**
	 * Whether the frame rate is in trouble: in the foreground, below Fraction of the cap the player chose, or of
	 * UncappedReference when they chose none (CapFps 0). The background cap never counts (Proposal 110).
	 */
	VEYRAUI_API bool IsPerformanceTroubled(bool bForeground, double FramesPerSecond, double CapFps, double UncappedReference, double Fraction);
}