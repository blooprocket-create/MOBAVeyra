// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Engine/DeveloperSettings.h"

#include "VeyraCameraSettings.generated.h"

/**
 * The default top-down view of a player's own Vanguard. Presentation, stored in
 * Config/DefaultGame.ini; players' camera settings (Settings Bible) arrive with the settings screen.
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
};
