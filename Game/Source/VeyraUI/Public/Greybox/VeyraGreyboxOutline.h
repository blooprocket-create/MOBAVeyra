// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Array.h"
#include "Shapes/VeyraShapes.h"

/** One straight piece of an outline, in world space. */
struct FVeyraOutlineSegment
{
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
};

/** The outline of a hit shape, for drawing it on the ground (ADR-009 §4 shapes). */
namespace VeyraGreyboxOutline
{
	/**
	 * Straight segments tracing Placed's edge at its origin's height: a circle as CircleSegments
	 * pieces, a sector as its share of them and its two sides, a rectangle as its four sides.
	 */
	VEYRAUI_API TArray<FVeyraOutlineSegment> Of(const FVeyraPlacedShape& Placed, int32 CircleSegments);
}
