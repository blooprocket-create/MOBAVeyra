// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Layout/VeyraLayout.h"
#include "Layout/VeyraRiver.h"

#include "Algo/Reverse.h"
#include "Tuning/VeyraWorldTuning.h"

double FVeyraWallShape::DepthInside(const FVector2D& Point) const
{
	return -VeyraWidthCurve::SignedDistance(Spine, Point);
}

TArray<FVeyraTerrainBox> FVeyraWallShape::Boxes() const
{
	// Its spine's distinct points: a span with no length has no direction to stand along.
	TArray<FVeyraCurveSample> Points;
	for (const FVeyraCurveSample& Sample : Spine)
	{
		if (Points.IsEmpty() || !Points.Last().Point.Equals(Sample.Point, UE_DOUBLE_KINDA_SMALL_NUMBER))
		{
			Points.Add(Sample);
		}
	}
	TArray<FVeyraTerrainBox> Out;
	if (Points.Num() < 2)
	{
		return Out;
	}
	const auto Direction = [&Points](int32 Span) { return (Points[Span + 1].Point - Points[Span].Point).GetSafeNormal(); };
	// Where the spine turns from From to To, how far past the joint a box must reach, in its half-thicknesses, for its
	// edge to meet the next box's on the outside of the bend: the tangent of half the turn.
	const auto Mitre = [](const FVector2D& From, const FVector2D& To) {
		return FMath::Abs(FVector2D::CrossProduct(From, To)) / (1.0 + FVector2D::DotProduct(From, To));
	};
	for (int32 Span = 0; Span + 1 < Points.Num(); ++Span)
	{
		const FVector2D& From = Points[Span].Point;
		const FVector2D& To = Points[Span + 1].Point;
		const FVector2D Along = Direction(Span);
		const double Thickness = FMath::Max(Points[Span].Width, Points[Span + 1].Width);
		const double Half = Thickness / 2.0;
		// Past each end of the spine by half its thickness, so its rounded tip stands inside; else to the mitre.
		const double Back = Span == 0 ? Half : Half * Mitre(Direction(Span - 1), Along);
		const double Ahead = Span + 2 == Points.Num() ? Half : Half * Mitre(Along, Direction(Span + 1));
		// A box faces across its thickness: here across the span.
		Out.Add({ (From + To) / 2.0 + Along * ((Ahead - Back) / 2.0), FVector2D(-Along.Y, Along.X), FVector2D::Distance(From, To) + Back + Ahead, Thickness });
	}
	return Out;
}

namespace VeyraLayout
{FVector2D ToVector(const FVeyraMapPoint& Point)
{
	return FVector2D(Point.X, Point.Y);
}

FVector2D Rotate(const FVector2D& Point)
{
	return VeyraRiver::Rotate(Point);
}

double Length(TConstArrayView<FVeyraMapPoint> Points)
{
	double Total = 0.0;
	for (int32 Index = 1; Index < Points.Num(); ++Index)
	{
		Total += FVector2D::Distance(ToVector(Points[Index - 1]), ToVector(Points[Index]));
	}
	return Total;
}

FVector2D PointAlong(TConstArrayView<FVeyraMapPoint> Points, double Distance)
{
	if (Points.IsEmpty())
	{
		return FVector2D::ZeroVector;
	}
	double Remaining = FMath::Max(0.0, Distance);
	for (int32 Index = 1; Index < Points.Num(); ++Index)
	{
		const FVector2D From = ToVector(Points[Index - 1]);
		const FVector2D To = ToVector(Points[Index]);
		const double Segment = FVector2D::Distance(From, To);
		if (Remaining <= Segment && Segment > 0.0)
		{
			return From + (To - From) * (Remaining / Segment);
		}
		Remaining -= Segment;
	}
	return ToVector(Points.Last());
}

double DistanceToPath(TConstArrayView<FVeyraMapPoint> Points, const FVector2D& Point)
{
	if (Points.Num() == 1)
	{
		return FVector2D::Distance(Point, ToVector(Points[0]));
	}
	double Nearest = TNumericLimits<double>::Max();
	for (int32 Index = 1; Index < Points.Num(); ++Index)
	{
		const FVector2D Closest = FMath::ClosestPointOnSegment2D(Point, ToVector(Points[Index - 1]), ToVector(Points[Index]));
		Nearest = FMath::Min(Nearest, FVector2D::Distance(Point, Closest));
	}
	return Nearest;
}

double DepthInTeamAHalf(const FVeyraBattlegroundLayout& Layout, const FVector2D& Point)
{
	// The distance from the line X + Y = 0, signed so Team A's Prime Well lies on the positive side.
	const double TeamASide = FMath::Sign(Layout.Base.PrimeWell.X + Layout.Base.PrimeWell.Y);
	return TeamASide * (Point.X + Point.Y) / UE_SQRT_2;
}

TArray<FVector2D> Waypoints(const FVeyraLaneLayout& Lane, EVeyraTeam Team)
{
	TArray<FVector2D> Path;
	for (const FVeyraMapPoint& Point : Lane.Points)
	{
		Path.Add(ToVector(Point));
	}
	if (Team == EVeyraTeam::B)
	{
		Algo::Reverse(Path);
	}
	return Path;
}

FVector2D FluxbornSpawnPoint(const FVeyraLaneLayout& Lane, EVeyraTeam Team)
{
	TArray<FVeyraMapPoint> Path = Lane.Points;
	if (Team == EVeyraTeam::B)
	{
		Algo::Reverse(Path);
	}
	return PointAlong(Path, Lane.FluxbornSpawnDistance);
}

FVector2D ForTeam(const FVector2D& TeamAPoint, EVeyraTeam Team)
{
	return Team == EVeyraTeam::B ? Rotate(TeamAPoint) : TeamAPoint;
}

FVector2D Fountain(const FVeyraBattlegroundLayout& Layout, EVeyraTeam Team)
{
	return ForTeam(ToVector(Layout.Base.Fountain), Team);
}

TArray<FVeyraFogPlacement> DenseFog(const FVeyraBattlegroundLayout& Layout)
{
	TArray<FVeyraFogPlacement> Fog;
	for (const EVeyraTeam Team : { EVeyraTeam::A, EVeyraTeam::B })
	{
		for (const FVeyraFogLayout& Circle : Layout.DenseFog)
		{
			Fog.Add(FVeyraFogPlacement{ ForTeam(ToVector(Circle.Center), Team), Circle.Radius });
		}
	}
	return Fog;
}

FVeyraWallShape WallShape(const FVeyraBattlegroundLayout& Layout, int32 Index, EVeyraTeam Team)
{
	FVeyraWallShape Shape;
	Shape.Team = Team;
	Shape.Index = Index;
	if (!Layout.Walls.IsValidIndex(Index))
	{
		return Shape;
	}
	// The rotation is linear, so the curve through rotated points is the rotated curve.
	TArray<FVector2D> Points;
	TArray<double> Widths;
	for (const FVeyraWallPoint& Point : Layout.Walls[Index].Points)
	{
		Points.Add(ForTeam(FVector2D(Point.X, Point.Y), Team));
		Widths.Add(Point.Width);
	}
	Shape.Spine = VeyraWidthCurve::Sample(Points, Widths, Layout.WallSamplesPerSegment);
	return Shape;
}

TArray<FVeyraWallShape> WallShapes(const FVeyraBattlegroundLayout& Layout)
{
	TArray<FVeyraWallShape> Out;
	for (const EVeyraTeam Team : { EVeyraTeam::A, EVeyraTeam::B })
	{
		for (int32 Index = 0; Index < Layout.Walls.Num(); ++Index)
		{
			Out.Add(WallShape(Layout, Index, Team));
		}
	}
	return Out;
}

TArray<FVeyraTerrainBox> Walls(const FVeyraBattlegroundLayout& Layout)
{
	TArray<FVeyraTerrainBox> Out;
	for (const FVeyraWallShape& Shape : WallShapes(Layout))
	{
		Out.Append(Shape.Boxes());
	}
	return Out;
}

TArray<FVeyraStructurePlacement> Structures(const FVeyraBattlegroundLayout& Layout)
{
	TArray<FVeyraStructurePlacement> Placements;
	for (const EVeyraTeam Team : { EVeyraTeam::A, EVeyraTeam::B })
	{
		for (const FVeyraLaneLayout& Lane : Layout.Lanes)
		{
			// Each team's structures stand the same distances along the lane from its own end.
			TArray<FVeyraMapPoint> Path = Lane.Points;
			if (Team == EVeyraTeam::B)
			{
				Algo::Reverse(Path);
			}
			// Outer Spire first: the order the lane's structures must fall in.
			const int32 Spires = Lane.SpireDistances.Num();
			for (int32 Index = Spires - 1; Index >= 0; --Index)
			{
				const FVector2D OnLane = PointAlong(Path, Lane.InhibitorDistance + Lane.SpireDistances[Index]);
				Placements.Add({ EVeyraStructureKind::LaneSpire, Team, Lane.Lane, Spires - 1 - Index, OnLane });
			}
			Placements.Add({ EVeyraStructureKind::Inhibitor, Team, Lane.Lane, Spires, PointAlong(Path, Lane.InhibitorDistance) });
		}
		for (int32 Index = 0; Index < Layout.Base.BaseTowers.Num(); ++Index)
		{
			Placements.Add({ EVeyraStructureKind::BaseTower, Team, {}, Index, ForTeam(ToVector(Layout.Base.BaseTowers[Index]), Team) });
		}
		Placements.Add({ EVeyraStructureKind::PrimeWell, Team, {}, 0, ForTeam(ToVector(Layout.Base.PrimeWell), Team) });
	}
	return Placements;
}

bool IsJungle(const FVeyraBattlegroundLayout& Layout, const FVector2D& Point)
{
	if (FMath::Abs(Point.X) > Layout.HalfExtent || FMath::Abs(Point.Y) > Layout.HalfExtent)
	{
		return false;
	}
	// Gameplay classification uses the same sampled river as world authoring.
	if (VeyraRiver::ShapeOf(Layout).IsWater(Point))
	{
		return false;
	}
	// Each lane's one path is both teams' road.
	for (const FVeyraLaneLayout& Lane : Layout.Lanes)
	{
		if (DistanceToPath(Lane.Points, Point) <= Lane.Width / 2.0)
		{
			return false;
		}
	}
	for (const EVeyraTeam Team : { EVeyraTeam::A, EVeyraTeam::B })
	{
		if (FVector2D::Distance(Point, ForTeam(ToVector(Layout.Base.PrimeWell), Team)) <= Layout.Base.PadRadius)
		{
			return false;
		}
	}
	return true;
}

bool RotatesOntoALane(const FVeyraLaneLayout& Lane, TConstArrayView<FVeyraLaneLayout> Lanes)
{
	const int32 Count = Lane.Points.Num();
	for (const FVeyraLaneLayout& Other : Lanes)
	{
		bool bMatches = Other.Points.Num() == Count;
		for (int32 Index = 0; bMatches && Index < Count; ++Index)
		{
			bMatches = Rotate(ToVector(Lane.Points[Index])).Equals(ToVector(Other.Points[Count - 1 - Index]), UE_DOUBLE_KINDA_SMALL_NUMBER);
		}
		if (bMatches)
		{
			return true;
		}
	}
	return false;
}
}
