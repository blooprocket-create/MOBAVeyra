// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Camera/VeyraCameraRules.h"
#include "Content/VeyraContentId.h"

class FVeyraSettingsStore;
class UVeyraCameraSettings;

/** The camera's behaviour as the player set it (Settings Bible §2, §12.3; ADR-024 §6). */
struct FVeyraCameraPreferences
{
	/** The mode a match starts in; the camera key's choice is kept for the next match. */
	EVeyraCameraMode DefaultMode = EVeyraCameraMode::Free;
	/** Camera Movement Speed and Edge-Scroll Speed, in units per second. */
	double PanSpeed = 0.0;
	double EdgeScrollSpeed = 0.0;
	/** Whether the screen's edges pan the camera, how near an edge, in pixels, and after how long there, in seconds (SET-85–87). */
	bool bEdgeScroll = true;
	double EdgeScrollPixels = 0.0;
	double EdgeDelaySeconds = 0.0;
	/** Camera Drag Sensitivity, in units per pixel dragged. */
	double DragUnitsPerPixel = 0.0;
	/** The camera goes to the Vanguard when it respawns (SET-156). */
	bool bReturnOnRespawn = true;
	/** A Locked or Semi-Locked camera may pan freely while the Vanguard waits to respawn (SET-157). */
	bool bFreeWhileDead = true;
};

/** The player's camera settings over the developer's (UVeyraCameraSettings), apart from the engine. */
namespace VeyraCameraPreferences
{
	/** The settings the camera reads, as the registry names them. */
	VEYRAMATCH_API const FVeyraContentId& DefaultMode();
	VEYRAMATCH_API const FVeyraContentId& MoveSpeed();
	VEYRAMATCH_API const FVeyraContentId& EdgeScroll();
	VEYRAMATCH_API const FVeyraContentId& EdgeScrollSpeed();
	VEYRAMATCH_API const FVeyraContentId& EdgeZone();
	VEYRAMATCH_API const FVeyraContentId& EdgeDelay();
	VEYRAMATCH_API const FVeyraContentId& DragSensitivity();
	VEYRAMATCH_API const FVeyraContentId& ReturnOnRespawn();
	VEYRAMATCH_API const FVeyraContentId& FreeWhileDead();

	/**
	 * The camera's behaviour: View's, with the player's settings in Store in their place; View's alone
	 * without a store (a server, a test, a game without settings).
	 */
	VEYRAMATCH_API FVeyraCameraPreferences Resolve(const UVeyraCameraSettings& View, const FVeyraSettingsStore* Store);

	/**
	 * A speed slider's value as a multiple of the developer's speed (ADR-024 §9.1): its default is 1,
	 * its minimum Slowest and its maximum Fastest, evenly in ratio between them.
	 */
	VEYRAMATCH_API double SpeedScale(double Value, double Minimum, double Default, double Maximum, double Slowest, double Fastest);

	/** The mode's name as the registry's options spell it, and back; unset for anything else. */
	VEYRAMATCH_API FString ModeName(EVeyraCameraMode Mode);
	VEYRAMATCH_API TOptional<EVeyraCameraMode> ParseMode(const FString& Name);
}
