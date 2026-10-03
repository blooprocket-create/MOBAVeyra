// Copyright © 2026 Wayfinder Studios. All rights reserved.
#pragma once

#include "CoreMinimal.h"

class UWorld;
struct FVeyraSurfaceTuning;

/** World-owned placement on static playable ground (ADR-040 §4). Failure never falls back to Z=0. */
namespace VeyraSurfacePlacement
{
	VEYRAWORLD_API bool Resolve(UWorld& World, const FVector2D& Point, double HalfHeight,
		const FVeyraSurfaceTuning& Settings, FVector& OutLocation);
}
