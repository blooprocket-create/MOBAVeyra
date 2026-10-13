// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * What a capture of the match HUD asks it to show (Veyra.Dev.HudHover and Veyra.Dev.StatsPanelOpen; ADR-080 §6): a deck
 * tile's tooltip as if the cursor rested on it, and the stats panel held open. Kept for the whole process, as the commands
 * that set it run at launch (Smoke.ps1 -ScreenshotCommands), before any match's HUD exists. Presentation only.
 */
namespace VeyraHudCapture
{
	/** Passive, Q, W, E, R or Item1 to Item6; empty for the cursor's own. */
	VEYRAUI_API const FString& PinnedHover();
	VEYRAUI_API void PinHover(const FString& Tile);

	VEYRAUI_API bool IsStatsPanelHeldOpen();
	VEYRAUI_API void HoldStatsPanelOpen(bool bOpen);
}
