// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Layout/VeyraLayout.h"

#include "Algo/Reverse.h"
#include "Tuning/VeyraWorldTuning.h"

namespace VeyraLayout
{
FVector2D ToVector(const FVeyraMapPoint& Point)
{
	return FVector2D(Point.X, Point.Y);
}

FVector2D Mirror(const FVector2D& Point)
{
	return FVector2D(-Point.Y, -Point.X);
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

FVector2D ForTeam(const FVector2D& TeamAPoint, EVeyraTeam Team)
{
	return Team == EVeyraTeam::B ? Mirror(TeamAPoint) : TeamAPoint;
}

FVector2D Fountain(const FVeyraBattlegroundLayout& Layout, EVeyraTeam Team)
{
	return ForTeam(ToVector(Layout.Base.Fountain), Team);
}

TArray<FVeyraStructurePlacement> Structures(const FVeyraBattlegroundLayout& Layout)
{
	TArray<FVeyraStructurePlacement> Placements;
	for (const EVeyraTeam Team : { EVeyraTeam::A, EVeyraTeam::B })
	{
		for (const FVeyraLaneLayout& Lane : Layout.Lanes)
		{
			// Outer Spire first: the order the lane's structures must fall in.
			const int32 Spires = Lane.SpireDistances.Num();
			for (int32 Index = Spires - 1; Index >= 0; --Index)
			{
				const FVector2D OnLane = PointAlong(Lane.Points, Lane.InhibitorDistance + Lane.SpireDistances[Index]);
				Placements.Add({ EVeyraStructureKind::LaneSpire, Team, Lane.Lane, Spires - 1 - Index, ForTeam(OnLane, Team) });
			}
			const FVector2D Inhibitor = PointAlong(Lane.Points, Lane.InhibitorDistance);
			Placements.Add({ EVeyraStructureKind::Inhibitor, Team, Lane.Lane, Spires, ForTeam(Inhibitor, Team) });
		}
		for (int32 Index = 0; Index < Layout.Base.BaseTowers.Num(); ++Index)
		{
			Placements.Add({ EVeyraStructureKind::BaseTower, Team, {}, Index, ForTeam(ToVector(Layout.Base.BaseTowers[Index]), Team) });
		}
		Placements.Add({ EVeyraStructureKind::PrimeWell, Team, {}, 0, ForTeam(ToVector(Layout.Base.PrimeWell), Team) });
	}
	return Placements;
}

bool MirrorsOntoItself(const FVeyraLaneLayout& Lane)
{
	const int32 Count = Lane.Points.Num();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVector2D Mirrored = Mirror(ToVector(Lane.Points[Index]));
		if (!Mirrored.Equals(ToVector(Lane.Points[Count - 1 - Index]), UE_DOUBLE_KINDA_SMALL_NUMBER))
		{
			return false;
		}
	}
	return true;
}
}
