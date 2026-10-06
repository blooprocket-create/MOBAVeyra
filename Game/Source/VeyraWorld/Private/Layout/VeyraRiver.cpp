// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Layout/VeyraRiver.h"

#include "Misc/ScopeLock.h"
#include "Tuning/VeyraWorldTuning.h"

namespace
{
	FVector2D PointOf(const FVeyraRiverPoint& Control)
	{
		return FVector2D(Control.X, Control.Y);
	}

	FVeyraRiverPoint Rotated(const FVeyraRiverPoint& Control)
	{
		FVeyraRiverPoint Out = Control;
		Out.X = -Control.X;
		Out.Y = -Control.Y;
		return Out;
	}

	/**
	 * The centreline through Controls as a Catmull-Rom curve, SamplesPerSegment samples to each span and the last control
	 * after them; widths change linearly along each span.
	 */
	TArray<FVeyraRiverSample> Sample(TConstArrayView<FVeyraRiverPoint> Controls, int32 SamplesPerSegment)
	{
		TArray<FVector2D> Points;
		TArray<double> Widths;
		for (const FVeyraRiverPoint& Control : Controls)
		{
			Points.Add(PointOf(Control));
			Widths.Add(Control.Width);
		}
		return VeyraWidthCurve::Sample(Points, Widths, SamplesPerSegment);
	}

	bool SamePoints(TConstArrayView<FVeyraRiverPoint> A, TConstArrayView<FVeyraRiverPoint> B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A[Index].X != B[Index].X || A[Index].Y != B[Index].Y || A[Index].Width != B[Index].Width)
			{
				return false;
			}
		}
		return true;
	}

	/** Whether two rivers have the same shape: the same channels, sampled alike. */
	bool SameShape(const FVeyraRiverLayout& A, const FVeyraRiverLayout& B)
	{
		if (A.SamplesPerSegment != B.SamplesPerSegment || !SamePoints(A.Main, B.Main) || A.Islands.Num() != B.Islands.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Islands.Num(); ++Index)
		{
			if (A.Islands[Index].Site != B.Islands[Index].Site || !SamePoints(A.Islands[Index].Channel, B.Islands[Index].Channel))
			{
				return false;
			}
		}
		return true;
	}
}

FVeyraRiverShape::FVeyraRiverShape(const FVeyraRiverLayout& River)
{
	// The main channel: Team B's half, its rotation, reversed to run in from the far end, then Team A's from the centre.
	TArray<FVeyraRiverPoint> Main;
	for (int32 Index = River.Main.Num() - 1; Index >= 1; --Index)
	{
		Main.Add(Rotated(River.Main[Index]));
	}
	Main.Append(River.Main);
	Channels.Add({ TEXT("Main"), INDEX_NONE, Sample(Main, River.SamplesPerSegment) });
	for (const FVeyraRiverIsland& Island : River.Islands)
	{
		Channels.Add({ TEXT("Island"), Island.Site, Sample(Island.Channel, River.SamplesPerSegment) });
		TArray<FVeyraRiverPoint> Turned;
		for (const FVeyraRiverPoint& Control : Island.Channel)
		{
			Turned.Add(Rotated(Control));
		}
		// The rotated island carries the rotated site, which the layout's validation requires to be a site too.
		Channels.Add({ TEXT("Island"), INDEX_NONE, Sample(Turned, River.SamplesPerSegment) });
	}
}

double FVeyraRiverShape::SignedDistance(const FVeyraRiverChannel& Channel, const FVector2D& Point)
{
	return VeyraWidthCurve::SignedDistance(Channel.Samples, Point);
}

double FVeyraRiverShape::SignedDistance(const FVector2D& Point) const
{
	double Nearest = TNumericLimits<double>::Max();
	for (const FVeyraRiverChannel& Channel : Channels)
	{
		Nearest = FMath::Min(Nearest, SignedDistance(Channel, Point));
	}
	return Nearest;
}

bool FVeyraRiverShape::IsOnIsland(const FVector2D& Point, double Reach, int32 Directions, double Step) const
{
	if (IsWater(Point) || Directions < 1 || Step <= 0.0)
	{
		return false;
	}
	for (int32 Direction = 0; Direction < Directions; ++Direction)
	{
		const double Angle = UE_DOUBLE_TWO_PI * Direction / Directions;
		const FVector2D Way(FMath::Cos(Angle), FMath::Sin(Angle));
		bool bReachesWater = false;
		for (double Along = Step; Along <= Reach && !bReachesWater; Along += Step)
		{
			bReachesWater = IsWater(Point + Way * Along);
		}
		if (!bReachesWater)
		{
			return false;
		}
	}
	return true;
}

const FVeyraRiverShape& VeyraRiver::ShapeOf(const FVeyraBattlegroundLayout& Layout)
{
	// Few distinct rivers ever exist in one run: the committed one and test fixtures. Each is built once and kept.
	struct FEntry
	{
		FVeyraRiverLayout River;
		TUniquePtr<FVeyraRiverShape> Shape;
	};
	static FCriticalSection Guard;
	static TArray<FEntry> Shapes;
	FScopeLock Lock(&Guard);
	for (const FEntry& Entry : Shapes)
	{
		if (SameShape(Entry.River, Layout.River))
		{
			return *Entry.Shape;
		}
	}
	FEntry& Added = Shapes.Add_GetRef({ Layout.River, MakeUnique<FVeyraRiverShape>(Layout.River) });
	return *Added.Shape;
}