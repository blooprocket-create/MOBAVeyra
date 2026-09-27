// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

class APlayerController;
class UCanvas;
class UVeyraGreyboxSubsystem;

/**
 * The grey-box HUD, drawn over whichever HUD the player has: each unit's overhead bars and
 * statuses, the match clock, and the player's panel (level, XP, skill points, ranks and cooldowns).
 * It draws what VeyraHud reads from replicated state.
 */
namespace VeyraGreyboxHud
{
	void Draw(UCanvas& Canvas, const UVeyraGreyboxSubsystem& Greybox, const APlayerController* Viewer);
}
