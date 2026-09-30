// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class APlayerController;
class AVeyraGameState;
class AVeyraPlayerState;
class UCanvas;
class UFont;
class UWorld;
class UVeyraGreyboxSettings;
struct FSlateFontInfo;
struct FVeyraInterfacePreferences;

/**
 * The in-match HUD's deck and strips, drawn on the canvas (ADR-008 §1). The Art Bible leaves the
 * in-game HUD open (v0.1 §9), so the layout follows League's, in the client's design language:
 * smoked surfaces with thin outlines and one accent.
 * - Along the bottom, the deck: the portrait with its level and XP, the passive, Q W E R with their
 *   key caps, ranks and cooldowns, Health and the resource, the Flux Spells, the vision tool, the
 *   items and Gold. Hovering a slot shows what it does.
 * - Along the top, the strip: each side's kills around the match clock, then pause, vote and AFK
 *   notices; Team Flux in the top left.
 * - While the Vanguard is dead, the screen dims and counts down to its return.
 */
namespace VeyraHudDeck
{
	/**
	 * Draws the deck, the top strip, Team Flux, the readouts the player asked for and the death shade for
	 * Viewer, whose participant is Own, at the player's HUD scale.
	 */
	void Draw(UCanvas& Canvas, const UVeyraGreyboxSettings& Settings, const FVeyraInterfacePreferences& Preferences, const UFont* Font, const UWorld& World,
		const AVeyraGameState& GameState, const APlayerController* Viewer, const AVeyraPlayerState* Own, double ServerNow);

	/** Draws Text large and centred a third of the way down, in Color: the match's end, for one. */
	void DrawHeadline(UCanvas& Canvas, const UVeyraGreyboxSettings& Settings, const UFont* Font, const FString& Text, const FLinearColor& Color);
}
