// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Terrain/VeyraTerrainBox.h"

#include "Terrain/VeyraRuntimeTerrain.h"

namespace
{
	/** The box's own frame: X along its facing (half its thickness), Y across it (half its length). */
	struct FLocalFrame
	{
		FVector2D Along;
		FVector2D Across;
		double HalfThickness = 0.0;
		double HalfLength = 0.0;
	};

	FLocalFrame FrameOf(const FVeyraTerrainBox& Box)
	{
		// A box facing nowhere faces +X.
		const FVector2D Normal = Box.Facing.GetSafeNormal();
		const FVector2D Along = Normal.IsZero() ? FVector2D(1.0, 0.0) : Normal;
		return { Along, FVector2D(-Along.Y, Along.X), Box.Thickness / 2.0, Box.Length / 2.0 };
	}

	FVector2D ToLocal(const FVeyraTerrainBox& Box, const FLocalFrame& Frame, const FVector2D& Point)
	{
		const FVector2D Offset = Point - Box.Centre;
		return FVector2D(FVector2D::DotProduct(Offset, Frame.Along), FVector2D::DotProduct(Offset, Frame.Across));
	}
}

FVeyraTerrainBox FVeyraTerrainBox::Of(const FVeyraWallRequest& Request)
{
	return { FVector2D(Request.Centre), FVector2D(Request.Facing), Request.Length, Request.Thickness };
}

TStaticArray<FVector2D, 4> FVeyraTerrainBox::Corners() const
{
	const FLocalFrame Frame = FrameOf(*this);
	const FVector2D A = Frame.Along * Frame.HalfThickness;
	const FVector2D B = Frame.Across * Frame.HalfLength;
	TStaticArray<FVector2D, 4> Out;
	Out[0] = Centre - A - B;
	Out[1] = Centre + A - B;
	Out[2] = Centre + A + B;
	Out[3] = Centre - A + B;
	return Out;
}

FBox2D FVeyraTerrainBox::Bounds() const
{
	FBox2D Out(ForceInit);
	for (const FVector2D& Corner : Corners())
	{
		Out += Corner;
	}
	return Out;
}

double FVeyraTerrainBox::DistanceTo(const FVector2D& Point) const
{
	const FLocalFrame Frame = FrameOf(*this);
	const FVector2D Local = ToLocal(*this, Frame, Point);
	const double OutsideAlong = FMath::Max(FMath::Abs(Local.X) - Frame.HalfThickness, 0.0);
	const double OutsideAcross = FMath::Max(FMath::Abs(Local.Y) - Frame.HalfLength, 0.0);
	return FMath::Sqrt(OutsideAlong * OutsideAlong + OutsideAcross * OutsideAcross);
}

bool FVeyraTerrainBox::Crosses(const FVector2D& From, const FVector2D& To) const
{
	// The slab test in the box's frame: the segment's parameter range inside each pair of edges.
	const FLocalFrame Frame = FrameOf(*this);
	const FVector2D Start = ToLocal(*this, Frame, From);
	const FVector2D Delta = ToLocal(*this, Frame, To) - Start;
	double Enter = 0.0;
	double Leave = 1.0;
	const double Starts[2] = { Start.X, Start.Y };
	const double Deltas[2] = { Delta.X, Delta.Y };
	const double Halves[2] = { Frame.HalfThickness, Frame.HalfLength };
	for (int32 Axis = 0; Axis < 2; ++Axis)
	{
		if (FMath::IsNearlyZero(Deltas[Axis]))
		{
			if (FMath::Abs(Starts[Axis]) > Halves[Axis])
			{
				return false;
			}
			continue;
		}
		double Near = (-Halves[Axis] - Starts[Axis]) / Deltas[Axis];
		double Far = (Halves[Axis] - Starts[Axis]) / Deltas[Axis];
		if (Near > Far)
		{
			Swap(Near, Far);
		}
		Enter = FMath::Max(Enter, Near);
		Leave = FMath::Min(Leave, Far);
		if (Enter > Leave)
		{
			return false;
		}
	}
	return true;
}

double FVeyraTerrainBox::DistanceToSegment(const FVector2D& From, const FVector2D& To) const
{
	if (Crosses(From, To))
	{
		return 0.0;
	}
	// Apart, the nearest pair is an end of the segment to the box, or a corner of the box to the segment.
	double Nearest = FMath::Min(DistanceTo(From), DistanceTo(To));
	for (const FVector2D& Corner : Corners())
	{
		Nearest = FMath::Min(Nearest, FVector2D::Distance(Corner, FMath::ClosestPointOnSegment2D(Corner, From, To)));
	}
	return Nearest;
}
