// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/** One sample of a curve with a width, as a river's channel or a wall's spine: where it passes and how wide it is there. */
struct FVeyraCurveSample
{
	FVector2D Point = FVector2D::ZeroVector;
	double Width = 0.0;
};

/**
 * A band along a curve (ADR-040 §6; ADR-043 §1): the river's channels and the battleground's walls are both one, authored
 * as control points with widths and sampled once.
 */
namespace VeyraWidthCurve
{
	/**
	 * The curve through Points as a Catmull-Rom spline, SamplesPerSegment samples to each span and the last point after
	 * them; widths change linearly along each span. Points and Widths pair up; empty when fewer than two points.
	 */
	VEYRAWORLD_API TArray<FVeyraCurveSample> Sample(TConstArrayView<FVector2D> Points, TConstArrayView<double> Widths, int32 SamplesPerSegment);

	/** How far Point lies from the band's edge: negative inside it. Its ends are rounded. */
	VEYRAWORLD_API double SignedDistance(TConstArrayView<FVeyraCurveSample> Samples, const FVector2D& Point);
}
