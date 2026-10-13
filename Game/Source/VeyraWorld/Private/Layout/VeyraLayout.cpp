// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Layout/VeyraLayout.h"
#include "Layout/VeyraRiver.h"

#include "Algo/AllOf.h"
#include "Algo/Reverse.h"
#include "CompGeom/PolygonTriangulation.h"
#include "IndexTypes.h"
#include "Tuning/VeyraWorldTuning.h"

namespace
{
	// A wall run's sharpest bend a mitre closes: past it the mitre's reach grows without bound, so it is held there.
	// Geometry, not tuning.
	constexpr double MaxMitreTurn = UE_DOUBLE_PI * 2.0 / 3.0;

	// An edge box through a neck of its mass is cut back to the neck's far side, measured again as its inner face moves
	// until it lies within a hundredth of a unit; however thin the neck, the box stays at least a unit deep, so it still
	// blocks. Geometry, not tuning.
	constexpr int32 NeckPasses = 8;
	constexpr double NeckSlack = 0.01;
	constexpr double MinEdgeDepth = 1.0;

	/** Which side of the line from A through B C lies: 1 left, -1 right, 0 on it. */
	int32 Turn(const FVector2D& A, const FVector2D& B, const FVector2D& C)
	{
		const double Cross = FVector2D::CrossProduct(B - A, C - A);
		return Cross > 0.0 ? 1 : (Cross < 0.0 ? -1 : 0);
	}

	/** Whether the spans AB and CD touch or cross. */
	bool SpansMeet(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D)
	{
		const int32 ABC = Turn(A, B, C);
		const int32 ABD = Turn(A, B, D);
		const int32 CDA = Turn(C, D, A);
		const int32 CDB = Turn(C, D, B);
		if (ABC != ABD && CDA != CDB)
		{
			return true;
		}
		// In line: they meet where an end of one lies on the other.
		const auto Spans = [](const FVector2D& From, const FVector2D& To, const FVector2D& Point) {
			return FMath::Min(From.X, To.X) <= Point.X && Point.X <= FMath::Max(From.X, To.X) && FMath::Min(From.Y, To.Y) <= Point.Y
				&& Point.Y <= FMath::Max(From.Y, To.Y);
		};
		return (ABC == 0 && Spans(A, B, C)) || (ABD == 0 && Spans(A, B, D)) || (CDA == 0 && Spans(C, D, A)) || (CDB == 0 && Spans(C, D, B));
	}

	/** Whether Polygon is simple: three corners or more, no span of no length, none folding back, and spans meeting only at shared corners. */
	bool IsSimpleImpl(TConstArrayView<FVector2D> Polygon)
	{
		const int32 Count = Polygon.Num();
		if (Count < 3 || !Algo::AllOf(Polygon, [](const FVector2D& Corner) { return FMath::IsFinite(Corner.X) && FMath::IsFinite(Corner.Y); }))
		{
			return false;
		}
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector2D& Before = Polygon[(Index + Count - 1) % Count];
			const FVector2D& Here = Polygon[Index];
			const FVector2D& After = Polygon[(Index + 1) % Count];
			if (Here == After || (Turn(Before, Here, After) == 0 && FVector2D::DotProduct(Here - Before, After - Here) < 0.0))
			{
				return false;
			}
		}
		for (int32 First = 0; First < Count; ++First)
		{
			for (int32 Second = First + 2; Second < Count; ++Second)
			{
				if (First == 0 && Second == Count - 1)
				{
					continue;
				}
				if (SpansMeet(Polygon[First], Polygon[First + 1], Polygon[Second], Polygon[(Second + 1) % Count]))
				{
					return false;
				}
			}
		}
		return true;
	}
}

double FVeyraMassShape::DepthInside(const FVector2D& Point) const
{
	return VeyraLayout::DepthInPolygon(Outline, Point);
}

TArray<FVeyraTerrainBox> FVeyraMassShape::EdgeBoxes(double Thickness, double MaxLength) const
{
	TArray<FVeyraTerrainBox> Out;
	if (Outline.Num() < 3 || Thickness <= 0.0 || MaxLength <= 0.0)
	{
		return Out;
	}
	for (int32 Corner = 0; Corner < Outline.Num(); ++Corner)
	{
		const FVector2D& From = Outline[Corner];
		const FVector2D& To = Outline[(Corner + 1) % Outline.Num()];
		const double Span = FVector2D::Distance(From, To);
		if (Span <= UE_DOUBLE_KINDA_SMALL_NUMBER)
		{
			continue;
		}
		const FVector2D Along = (To - From) / Span;
		// Counter-clockwise, so the mass lies to the left of each span: each box faces inward from the edge.
		const FVector2D Inward(-Along.Y, Along.X);
		const int32 Pieces = FMath::Max(1, FMath::CeilToInt32(Span / MaxLength));
		for (int32 Piece = 0; Piece < Pieces; ++Piece)
		{
			const FVector2D A = From + Along * (Span * Piece / Pieces);
			const FVector2D B = From + Along * (Span * (Piece + 1) / Pieces);
			// Where the mass is a neck thinner than the box is deep, the box stops at its far side, never standing out.
			double Deep = Thickness;
			for (int32 Pass = 0; Pass < NeckPasses; ++Pass)
			{
				const double Beyond = FMath::Min3(DepthInside(A + Inward * Deep), DepthInside((A + B) / 2.0 + Inward * Deep), DepthInside(B + Inward * Deep));
				if (Beyond >= -NeckSlack / 2.0)
				{
					break;
				}
				Deep = FMath::Max(Deep + Beyond - NeckSlack / 2.0, MinEdgeDepth);
			}
			Out.Add({ (A + B) / 2.0 + Inward * (Deep / 2.0), Inward, Span / Pieces, Deep });
		}
	}
	return Out;
}

TArray<FVector2D> FVeyraMassShape::Triangles() const
{
	TArray<UE::Geometry::FIndex3i> Corners;
	PolygonTriangulation::TriangulateSimplePolygon<double>(Outline, Corners, /*bOrientAsHoleFill*/ false);
	TArray<FVector2D> Out;
	Out.Reserve(Corners.Num() * 3);
	for (const UE::Geometry::FIndex3i& Triangle : Corners)
	{
		Out.Append({ Outline[Triangle.A], Outline[Triangle.B], Outline[Triangle.C] });
	}
	return Out;
}

namespace VeyraLayout
{
FVector2D ToVector(const FVeyraMapPoint& Point)
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

bool IsInPolygon(TConstArrayView<FVector2D> Polygon, const FVector2D& Point)
{
	// Even-odd crossings of a ray toward +X.
	bool bInside = false;
	for (int32 Index = 0, Previous = Polygon.Num() - 1; Index < Polygon.Num(); Previous = Index++)
	{
		const FVector2D& A = Polygon[Index];
		const FVector2D& B = Polygon[Previous];
		if ((A.Y > Point.Y) != (B.Y > Point.Y) && Point.X < (B.X - A.X) * (Point.Y - A.Y) / (B.Y - A.Y) + A.X)
		{
			bInside = !bInside;
		}
	}
	return bInside;
}

bool IsSimplePolygon(TConstArrayView<FVector2D> Polygon)
{
	return IsSimpleImpl(Polygon);
}

double DepthInPolygon(TConstArrayView<FVector2D> Polygon, const FVector2D& Point)
{
	if (Polygon.IsEmpty())
	{
		return -TNumericLimits<double>::Max();
	}
	double Nearest = TNumericLimits<double>::Max();
	for (int32 Index = 0; Index < Polygon.Num(); ++Index)
	{
		const FVector2D Closest = FMath::ClosestPointOnSegment2D(Point, Polygon[Index], Polygon[(Index + 1) % Polygon.Num()]);
		Nearest = FMath::Min(Nearest, FVector2D::Distance(Point, Closest));
	}
	return IsInPolygon(Polygon, Point) ? Nearest : -Nearest;
}

double PlayableHalfExtent(const FVeyraBattlegroundLayout& Layout)
{
	return Layout.HalfExtent - Layout.Terrain.BoundaryWidth;
}

TArray<FVeyraTerrainBox> Rim(const FVeyraBattlegroundLayout& Layout)
{
	TArray<FVeyraTerrainBox> Out;
	const double Half = PlayableHalfExtent(Layout);
	const double Thickness = Layout.MassRules.EdgeThickness;
	const double MaxLength = Layout.MassRules.EdgeMaxLength;
	if (Half <= 0.0 || Thickness <= 0.0 || MaxLength <= 0.0)
	{
		return Out;
	}
	// Each side of the playable square, its boxes just outside it, facing in; they run on past the corners to close them.
	const double Side = 2.0 * (Half + Thickness);
	const int32 Pieces = FMath::Max(1, FMath::CeilToInt32(Side / MaxLength));
	for (const FVector2D& Outward : { FVector2D(1.0, 0.0), FVector2D(0.0, 1.0), FVector2D(-1.0, 0.0), FVector2D(0.0, -1.0) })
	{
		const FVector2D Along(-Outward.Y, Outward.X);
		const FVector2D Middle = Outward * (Half + Thickness / 2.0);
		for (int32 Piece = 0; Piece < Pieces; ++Piece)
		{
			const double Offset = (Piece + 0.5) * Side / Pieces - Side / 2.0;
			Out.Add({ Middle + Along * Offset, -Outward, Side / Pieces, Thickness });
		}
	}
	return Out;
}

TArray<FVeyraMassShape> Masses(const FVeyraBattlegroundLayout& Layout)
{
	TArray<FVeyraMassShape> Out;
	for (int32 Index = 0; Index < Layout.Masses.Items.Num(); ++Index)
	{
		const FVeyraMassLayout& Mass = Layout.Masses.Items[Index];
		FVeyraMassShape& Shape = Out.AddDefaulted_GetRef();
		Shape.Index = Index;
		Shape.Kind = Mass.Kind;
		for (const FVeyraMapPoint& Point : Mass.Points)
		{
			Shape.Outline.Add(ToVector(Point));
		}
	}
	return Out;
}

TArray<FVector2D> KeepFloor(const FVeyraBattlegroundLayout& Layout, EVeyraTeam Team)
{
	TArray<FVector2D> Floor;
	for (const FVeyraMapPoint& Point : Layout.Keep.Floor)
	{
		Floor.Add(ForTeam(ToVector(Point), Team));
	}
	return Floor;
}

bool IsInKeep(const FVeyraBattlegroundLayout& Layout, const FVector2D& Point)
{
	return IsInPolygon(KeepFloor(Layout, EVeyraTeam::A), Point) || IsInPolygon(KeepFloor(Layout, EVeyraTeam::B), Point);
}

TArray<FVeyraTerrainBox> KeepWallRun(const FVeyraKeepWallLayout& Run, EVeyraTeam Team)
{
	TArray<FVeyraTerrainBox> Out;
	const double Half = Run.Thickness / 2.0;
	const int32 Count = Run.Points.Num();
	// Past each inner joint as far as its mitre, half the wall times the tangent of half the turn, so the corner's outside
	// closes and nothing reaches beyond it; a run's ends stop at its gates.
	const auto Mitre = [&](int32 Joint) {
		if (Joint <= 0 || Joint >= Count - 1)
		{
			return 0.0;
		}
		const FVector2D In = ToVector(Run.Points[Joint]) - ToVector(Run.Points[Joint - 1]);
		const FVector2D Out = ToVector(Run.Points[Joint + 1]) - ToVector(Run.Points[Joint]);
		const double Turn = FMath::Abs(FMath::Atan2(FVector2D::CrossProduct(In, Out), FVector2D::DotProduct(In, Out)));
		return Half * FMath::Tan(FMath::Min(Turn, MaxMitreTurn) / 2.0);
	};
	for (int32 Span = 0; Span + 1 < Count; ++Span)
	{
		const FVector2D From = ForTeam(ToVector(Run.Points[Span]), Team);
		const FVector2D To = ForTeam(ToVector(Run.Points[Span + 1]), Team);
		const double Length = FVector2D::Distance(From, To);
		if (Length <= UE_DOUBLE_KINDA_SMALL_NUMBER)
		{
			continue;
		}
		const FVector2D Along = (To - From) / Length;
		const double Back = Mitre(Span);
		const double Ahead = Mitre(Span + 1);
		Out.Add({ (From + To) / 2.0 + Along * ((Ahead - Back) / 2.0), FVector2D(-Along.Y, Along.X), Length + Back + Ahead, Run.Thickness });
	}
	return Out;
}

TArray<FVeyraTerrainBox> KeepWalls(const FVeyraBattlegroundLayout& Layout)
{
	TArray<FVeyraTerrainBox> Out;
	for (const EVeyraTeam Team : { EVeyraTeam::A, EVeyraTeam::B })
	{
		for (const FVeyraKeepWallLayout& Run : Layout.Keep.Walls)
		{
			Out.Append(KeepWallRun(Run, Team));
		}
	}
	return Out;
}

TArray<FVeyraTerrainBox> Walls(const FVeyraBattlegroundLayout& Layout)
{
	TArray<FVeyraTerrainBox> Out;
	for (const FVeyraMassShape& Mass : Masses(Layout))
	{
		Out.Append(Mass.EdgeBoxes(Layout.MassRules.EdgeThickness, Layout.MassRules.EdgeMaxLength));
	}
	Out.Append(KeepWalls(Layout));
	Out.Append(Rim(Layout));
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
				const double Distance = Lane.InhibitorDistance + Lane.SpireDistances[Index];
				const FVector2D OnLane = PointAlong(Path, Distance);
				// Beside the road, on its side as this team walks the lane: the half turn keeps both teams' towers alike.
				const FVector2D Ahead = (PointAlong(Path, Distance + 5.0) - PointAlong(Path, FMath::Max(0.0, Distance - 5.0))).GetSafeNormal();
				const int32 Side = Lane.SpireSides.IsValidIndex(Index) ? Lane.SpireSides[Index] : 0;
				const FVector2D Aside = FVector2D(-Ahead.Y, Ahead.X) * (Side * Layout.SpireOffset);
				Placements.Add({ EVeyraStructureKind::LaneSpire, Team, Lane.Lane, Spires - 1 - Index, OnLane + Aside });
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
	return !IsInKeep(Layout, Point);
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
