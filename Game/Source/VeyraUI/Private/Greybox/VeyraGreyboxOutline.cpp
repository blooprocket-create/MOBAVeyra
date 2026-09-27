// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Greybox/VeyraGreyboxOutline.h"

namespace
{
	/** Points along an arc of Radius about Origin, from FromDegrees to ToDegrees off Forward, in Pieces steps. */
	void AddOutlineArc(TArray<FVeyraOutlineSegment>& Segments, const FVector& Origin, const FVector& Forward, double Radius, double FromDegrees, double ToDegrees,
		int32 Pieces)
	{
		const FVector Up = FVector::UpVector;
		FVector Previous = Origin + Forward.RotateAngleAxis(FromDegrees, Up) * Radius;
		for (int32 Piece = 1; Piece <= Pieces; ++Piece)
		{
			const double Degrees = FMath::Lerp(FromDegrees, ToDegrees, static_cast<double>(Piece) / Pieces);
			const FVector Next = Origin + Forward.RotateAngleAxis(Degrees, Up) * Radius;
			Segments.Add(FVeyraOutlineSegment{ Previous, Next });
			Previous = Next;
		}
	}
}

TArray<FVeyraOutlineSegment> VeyraGreyboxOutline::Of(const FVeyraPlacedShape& Placed, int32 CircleSegments)
{
	constexpr double FullTurnDegrees = 360.0;
	const FVector Forward = Placed.Direction.GetSafeNormal2D().IsNearlyZero() ? FVector::ForwardVector : Placed.Direction.GetSafeNormal2D();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	const FVector& Origin = Placed.Origin;
	const FVeyraShape& Shape = Placed.Shape;
	const int32 Pieces = FMath::Max(3, CircleSegments);

	TArray<FVeyraOutlineSegment> Segments;
	switch (Shape.Kind)
	{
	case EVeyraShapeKind::Circle:
		AddOutlineArc(Segments, Origin, Forward, Shape.Radius, 0.0, FullTurnDegrees, Pieces);
		break;
	case EVeyraShapeKind::Sector:
	{
		// Centred on the direction; its share of a circle's pieces, at least one.
		const double Half = Shape.ArcDegrees / 2.0;
		const int32 ArcPieces = FMath::Max(1, FMath::CeilToInt32(Pieces * Shape.ArcDegrees / FullTurnDegrees));
		AddOutlineArc(Segments, Origin, Forward, Shape.Radius, -Half, Half, ArcPieces);
		if (Shape.ArcDegrees < FullTurnDegrees)
		{
			Segments.Add(FVeyraOutlineSegment{ Origin, Origin + Forward.RotateAngleAxis(-Half, FVector::UpVector) * Shape.Radius });
			Segments.Add(FVeyraOutlineSegment{ Origin, Origin + Forward.RotateAngleAxis(Half, FVector::UpVector) * Shape.Radius });
		}
		break;
	}
	case EVeyraShapeKind::Rectangle:
	{
		// From the origin along the direction, centred across it.
		const FVector Across = Right * (Shape.Width / 2.0);
		const FVector Along = Forward * Shape.Length;
		const TArray<FVector, TInlineAllocator<4>> Corners = { Origin - Across, Origin + Across, Origin + Along + Across, Origin + Along - Across };
		for (int32 Corner = 0; Corner < Corners.Num(); ++Corner)
		{
			Segments.Add(FVeyraOutlineSegment{ Corners[Corner], Corners[(Corner + 1) % Corners.Num()] });
		}
		break;
	}
	}
	return Segments;
}
