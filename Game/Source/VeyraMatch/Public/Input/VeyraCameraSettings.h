// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Camera/VeyraCameraRules.h"
#include "Engine/DeveloperSettings.h"

#include "VeyraCameraSettings.generated.h"

/**
 * The local camera (Settings Bible §2; ADR-020 §1). Presentation, stored in Config/DefaultGame.ini;
 * players' own camera settings arrive with the settings screen.
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

	/** How fast the camera pans at the screen's edge or on its keys, in units per second. */
	UPROPERTY(Config, EditAnywhere, Category = "Movement", meta = (ClampMin = "0"))
	float PanSpeed = 0.0f;

	/** Whether the screen's edges pan the camera, and how near an edge the cursor must be, in pixels. */
	UPROPERTY(Config, EditAnywhere, Category = "Movement")
	bool bEdgeScroll = true;

	UPROPERTY(Config, EditAnywhere, Category = "Movement", meta = (ClampMin = "0"))
	float EdgeScrollPixels = 0.0f;

	/** How far the view moves per pixel of middle-mouse drag, in units. */
	UPROPERTY(Config, EditAnywhere, Category = "Movement", meta = (ClampMin = "0"))
	float DragUnitsPerPixel = 0.0f;

	/** How far off the Vanguard a Semi-Locked camera may look, in units. */
	UPROPERTY(Config, EditAnywhere, Category = "Movement", meta = (ClampMin = "0"))
	float SemiLockedMaxOffset = 0.0f;
};
