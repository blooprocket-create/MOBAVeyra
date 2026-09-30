// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Camera/VeyraCameraRules.h"
#include "Engine/DeveloperSettings.h"

#include "VeyraCameraSettings.generated.h"

/**
 * The local camera (Settings Bible §2; ADR-020 §1). Presentation, stored in Config/DefaultGame.ini.
 * These are the developer's defaults; the player's camera settings (ADR-024 §6) scale and replace them
 * through VeyraCameraPreferences.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Veyra Camera"))
class VEYRAMATCH_API UVeyraCameraSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Distance from the Vanguard to the camera, in units. */
	UPROPERTY(Config, EditAnywhere, Category = "View", meta = (ClampMin = "1"))
	float Distance = 0.0f;

	/** The camera's downward angle; negative looks down. */
	UPROPERTY(Config, EditAnywhere, Category = "View", meta = (ClampMin = "-89", ClampMax = "0"))
	float PitchDegrees = 0.0f;

	/** The mode a match starts in: Free, as the Settings Bible sets it. */
	UPROPERTY(Config, EditAnywhere, Category = "Movement")
	EVeyraCameraMode DefaultMode = EVeyraCameraMode::Free;

	/** Camera Movement Speed: how fast the camera keys pan it, in units per second (Settings Bible §12.3). */
	UPROPERTY(Config, EditAnywhere, Category = "Movement", meta = (ClampMin = "0"))
	float PanSpeed = 0.0f;

	/** Edge-Scroll Speed: how fast the screen's edges pan it, in units per second (Settings Bible §12.3). */
	UPROPERTY(Config, EditAnywhere, Category = "Movement", meta = (ClampMin = "0"))
	float EdgeScrollSpeed = 0.0f;

	/** Whether the screen's edges pan the camera, and how near an edge the cursor must be, in pixels. */
	UPROPERTY(Config, EditAnywhere, Category = "Movement")
	bool bEdgeScroll = true;

	UPROPERTY(Config, EditAnywhere, Category = "Movement", meta = (ClampMin = "0"))
	float EdgeScrollPixels = 0.0f;

	/** How far the view moves per pixel of middle-mouse drag, in units. */
	UPROPERTY(Config, EditAnywhere, Category = "Movement", meta = (ClampMin = "0"))
	float DragUnitsPerPixel = 0.0f;

	/**
	 * What the player's speed sliders reach at their ends, as multiples of the speeds above: the
	 * slider's minimum is Slowest, its default 1, its maximum Fastest (ADR-024 §9.1).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Player settings", meta = (ClampMin = "0.01", ClampMax = "1"))
	float SpeedSettingSlowest = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Player settings", meta = (ClampMin = "1"))
	float SpeedSettingFastest = 0.0f;

	/** The Edge-Scroll Activation Zone's options in pixels from the edge (SET-86; ADR-024 §9.2), by option. */
	UPROPERTY(Config, EditAnywhere, Category = "Player settings")
	TMap<FString, float> EdgeZonePixels;

	/** The Edge-Scroll Delay's options in seconds the cursor rests at the edge first (SET-87; ADR-024 §9.2), by option. */
	UPROPERTY(Config, EditAnywhere, Category = "Player settings")
	TMap<FString, float> EdgeDelaySeconds;

	/** How far off the Vanguard a Semi-Locked camera may look, in units. */
	UPROPERTY(Config, EditAnywhere, Category = "Movement", meta = (ClampMin = "0"))
	float SemiLockedMaxOffset = 0.0f;

	/** How long the end-of-match pan to the fallen Prime Well takes, in seconds (ADR-020 §1). */
	UPROPERTY(Config, EditAnywhere, Category = "Movement", meta = (ClampMin = "0"))
	float EndPanSeconds = 0.0f;
};
