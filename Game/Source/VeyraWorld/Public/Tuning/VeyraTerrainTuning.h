// Copyright © 2026 Wayfinder Studios. All rights reserved.
#pragma once
#include "CoreMinimal.h"
#include "VeyraTerrainTuning.generated.h"

/** Authored river control: XY, full width; mirrored branch is derived, never copied. */
USTRUCT()
struct FVeyraRiverControl
{
	GENERATED_BODY()
	UPROPERTY() double X = 0.0;
	UPROPERTY() double Y = 0.0;
	UPROPERTY() double Width = 0.0;
};

/** Gameplay-significant production terrain parameters, all required in World.json. */
USTRUCT()
struct FVeyraTerrainTuning
{
	GENERATED_BODY()
	UPROPERTY() TArray<FVeyraRiverControl> RiverControls;
	UPROPERTY() int32 RiverSamplesPerSegment = 0;
	UPROPERTY() double RiverSurfaceZ = 0.0;
	UPROPERTY() double RiverBedZ = 0.0;
	UPROPERTY() double RiverFlowSpeed = 0.0;
	UPROPERTY() double LaneZ = 0.0;
	UPROPERTY() double BaseZ = 0.0;
	UPROPERTY() double JungleZ = 0.0;
	UPROPERTY() double BankBlend = 0.0;
	UPROPERTY() double LaneShoulder = 0.0;
	UPROPERTY() double ExteriorWidth = 0.0;
	UPROPERTY() double ExteriorZ = 0.0;
	UPROPERTY() double WallFootingClearance = 0.0;
};
