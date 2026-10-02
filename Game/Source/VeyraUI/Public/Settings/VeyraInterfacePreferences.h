// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Misc/Optional.h"

class FVeyraSettingsStore;
class UObject;
class UVeyraGreyboxSettings;

/** The HUD, the minimap and the in-match controls as the player set them (Settings Bible §3; ADR-024 §6). */
struct FVeyraInterfacePreferences
{
	/** The HUD deck's size, as a multiple of its designed size. */
	float HudScale = 1.0f;
	/** The minimap's side and its icons' sides, in pixels. */
	float MinimapSize = 0.0f;
	float MinimapVanguardIcon = 0.0f;
	float MinimapStructureIcon = 0.0f;
	float MinimapUnitIcon = 0.0f;
	/** Clicking the minimap moves the camera; right-clicking it sends the Vanguard (§3.2). */
	bool bMinimapClickMovesCamera = true;
	bool bMinimapRightClickMoves = true;
	/** How long a ping stays on the battleground and the minimap, in seconds. */
	float PingSeconds = 0.0f;
	/** The frame rate and ping readouts (§3.6). */
	bool bShowFps = false;
	bool bShowPing = false;
	/** The scoreboard's key switches it with each press instead of showing it while held (SET-56). */
	bool bScoreboardToggles = false;
	/** The cursor stays inside the game's window during a match (SET-83). */
	bool bConfineCursor = true;
	/** The local player's indicator outline, in units: Standard or Thick (Settings Bible §3.3; ADR-041 §2). */
	float IndicatorThickness = 0.0f;
	/** The chat's type size (SET-66), its backdrop, clear for Transparent (SET-67), how long a line stays whole, and its timestamps (Settings Bible §5.2). */
	int32 ChatFontSize = 0;
	FLinearColor ChatBackdrop = FLinearColor::Transparent;
	float ChatFadeSeconds = 0.0f;
	bool bChatTimestamps = false;
};

/** The player's interface settings over the developer's (UVeyraGreyboxSettings), apart from the engine. */
namespace VeyraInterfacePreferences
{
	/** The settings the interface reads, as the registry names them. */
	VEYRAUI_API const FVeyraContentId& HudScale();
	VEYRAUI_API const FVeyraContentId& MinimapScale();
	VEYRAUI_API const FVeyraContentId& MinimapIconScale();
	VEYRAUI_API const FVeyraContentId& MinimapClickMovesCamera();
	VEYRAUI_API const FVeyraContentId& MinimapRightClickMoves();
	VEYRAUI_API const FVeyraContentId& PingSeconds();
	VEYRAUI_API const FVeyraContentId& ShowFps();
	VEYRAUI_API const FVeyraContentId& ShowPing();
	VEYRAUI_API const FVeyraContentId& ScoreboardMode();
	VEYRAUI_API const FVeyraContentId& ConfineCursor();
	VEYRAUI_API const FVeyraContentId& IndicatorBoundary();
	VEYRAUI_API const FVeyraContentId& ChatTextSize();
	VEYRAUI_API const FVeyraContentId& ChatBackdrop();
	VEYRAUI_API const FVeyraContentId& ChatFadeSeconds();
	VEYRAUI_API const FVeyraContentId& ChatTimestamps();

	/** The interface: Hud's, with the player's settings in Store in their place; Hud's alone without a store. */
	VEYRAUI_API FVeyraInterfacePreferences Resolve(const UVeyraGreyboxSettings& Hud, const FVeyraSettingsStore* Store);

	/** The player's settings in WorldContext's game instance; null where there are none. */
	VEYRAUI_API const FVeyraSettingsStore* StoreOf(const UObject* WorldContext);

	/** The readouts the player asked for, such as "60 FPS" and "35 ms"; empty for none. The ping shows only once known. */
	VEYRAUI_API FString DescribeReadouts(const FVeyraInterfacePreferences& Preferences, float FramesPerSecond, TOptional<float> PingMilliseconds);
}
