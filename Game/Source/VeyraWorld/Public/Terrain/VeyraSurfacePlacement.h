// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class UWorld;
struct FVeyraSurfaceTuning;

/** Where the battleground's things stand (ADR-040 §4): on its playable ground, never on a wall, never at an assumed Z=0. */
namespace VeyraSurfacePlacement
{
	/**
	 * Where a body HalfHeight tall stands at Point: the ground's surface between the layout's surface bounds raised by
	 * HalfHeight. False where there is no ground there, or it is steeper than the surface allows; never a fallback height.
	 */
	VEYRAWORLD_API bool Resolve(const UWorld& World, const FVector2D& Point, double HalfHeight, const FVeyraSurfaceTuning& Settings, FVector& OutLocation);
}
