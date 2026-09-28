// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

class APlayerController;
class UCanvas;
class UVeyraGreyboxSubsystem;

/**
 * The grey-box HUD, drawn on the canvas of whichever HUD the player has (AVeyraHudOverlay): each
 * unit's overhead bars and statuses, the match clock, and the player's panel (the Vanguard, level,
 * XP, skill points, its passive, and each ability's rank, cooldown, waiting empowerment and what it
 * does, VeyraContentText). It draws what VeyraHud reads from replicated state.
 */
namespace VeyraGreyboxHud
{
	void Draw(UCanvas& Canvas, const UVeyraGreyboxSubsystem& Greybox, const APlayerController* Viewer);
}
