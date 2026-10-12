// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"
#include "Algo/Count.h"
#include "Layout/VeyraLayout.h"
#include "Layout/VeyraRiver.h"
#include "Tuning/VeyraWorldTuning.h"

namespace VeyraWorld
{
namespace
{
	// How finely validation probes geometry: the river around a Well, in directions and units a step, and a mass's outline,
	// in units a step. Resolution of the checks, not tuning.
	constexpr int32 IslandProbeDirections = 24;
	constexpr double IslandProbeStep = 100.0;
	constexpr double OutlineProbeStep = 50.0;

	// The mass generator writes whole units: an outline may stand this much beyond where its simplification put it, and
	// what it writes as a half turn or on a line matches within it.
	constexpr double OutlineRounding = 1.0;

	// The most samples a river span may take, as the schema allows.
	constexpr int32 MaxRiverSamplesPerSegment = 64;

	// A SHA-256 written as hex.
	constexpr int32 Sha256HexLength = 64;

	bool IsOnFloor(const FVeyraMapPoint& Point, double HalfExtent)
	{
		return FMath::Abs(Point.X) <= HalfExtent && FMath::Abs(Point.Y) <= HalfExtent;
	}

	bool IsFinite(const FVector2D& Point)
	{
		return FMath::IsFinite(Point.X) && FMath::IsFinite(Point.Y);
	}

	TArray<FVector2D> ToVectors(TConstArrayView<FVeyraMapPoint> Points)
	{
		TArray<FVector2D> Out;
		for (const FVeyraMapPoint& Point : Points)
		{
			Out.Add(VeyraLayout::ToVector(Point));
		}
		return Out;
	}

	double BodyRadius(const FVeyraWorldTuning& Tuning, EVeyraStructureKind Kind)
	{
		switch (Kind)
		{
		case EVeyraStructureKind::LaneSpire:
			return Tuning.Structures.LaneSpire.CapsuleRadius;
		case EVeyraStructureKind::BaseTower:
			return Tuning.Structures.BaseTower.CapsuleRadius;
		case EVeyraStructureKind::Inhibitor:
			return Tuning.Structures.Inhibitor.CapsuleRadius;
		case EVeyraStructureKind::PrimeWell:
			return Tuning.Structures.PrimeWell.CapsuleRadius;
		}
		return 0.0;
	}

	/** Twice Polygon's signed area: positive when its corners run counter-clockwise. */
	double DoubleArea(TConstArrayView<FVector2D> Polygon)
	{
		double Sum = 0.0;
		for (int32 Index = 0; Index < Polygon.Num(); ++Index)
		{
			Sum += FVector2D::CrossProduct(Polygon[Index], Polygon[(Index + 1) % Polygon.Num()]);
		}
		return Sum;
	}

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
	bool IsSimple(TConstArrayView<FVector2D> Polygon)
	{
		const int32 Count = Polygon.Num();
		if (Count < 3 || !Algo::AllOf(Polygon, [](const FVector2D& Corner) { return IsFinite(Corner); }))
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

	/** Whether Points makes a path: two points or more, each finite and apart from the one before. */
	bool IsPath(TConstArrayView<FVeyraMapPoint> Points)
	{
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			const FVector2D Here = VeyraLayout::ToVector(Points[Index]);
			if (!IsFinite(Here) || (Index > 0 && Here == VeyraLayout::ToVector(Points[Index - 1])))
			{
				return false;
			}
		}
		return Points.Num() >= 2;
	}

	/**
	 * Both teams' open spaces as the mass generator subtracts them (ADR-073 §2): each lane's road and shoulders, the
	 * river's walkable water and banks, the keeps, the clearings and the routes. How deep a point lies inside the deepest.
	 */
	class FOpenSpaces
	{
	public:
		explicit FOpenSpaces(const FVeyraBattlegroundLayout& Layout)
		{
			for (const FVeyraLaneLayout& Lane : Layout.Lanes)
			{
				const TArray<FVector2D> Path = ToVectors(Lane.Points);
				AddBand(FString::Printf(TEXT("the %s lane's road and shoulders"), *UEnum::GetValueAsString(Lane.Lane)), Path,
					[&](int32) { return Lane.Width / 2.0 + Layout.Terrain.LaneShoulder; });
			}
			const FVeyraRiverShape& River = VeyraRiver::ShapeOf(Layout);
			const auto AddWater = [&](const FString& Name, TConstArrayView<FVeyraRiverSample> Samples) {
				TArray<FVector2D> Path;
				for (const FVeyraRiverSample& Sample : Samples)
				{
					Path.Add(Sample.Point);
				}
				AddBand(Name, Path, [&](int32 Span) { return FMath::Max(Samples[Span].Width, Samples[Span + 1].Width) / 2.0 + Layout.MassRules.RiverMargin; });
			};
			AddWater(TEXT("the river's walkable water and banks"), River.GetWalkableMain());
			// The main channel comes first; every channel after it rounds an island, and all its water is ground.
			for (int32 Channel = 1; Channel < River.GetChannels().Num(); ++Channel)
			{
				AddWater(TEXT("an island channel's water and banks"), River.GetChannels()[Channel].Samples);
			}
			for (const EVeyraTeam Team : { EVeyraTeam::A, EVeyraTeam::B })
			{
				const FString Whose = Team == EVeyraTeam::A ? TEXT("Team A's") : TEXT("Team B's");
				FShape& Keep = Shapes.AddDefaulted_GetRef();
				Keep.Name = Whose + TEXT(" keep");
				Keep.Form = EForm::Polygon;
				Keep.Points = VeyraLayout::KeepFloor(Layout, Team);
				Keep.Bounds = FBox2D(Keep.Points);
				for (const FVeyraClearingLayout& Clearing : Layout.Clearings)
				{
					FShape& Disc = Shapes.AddDefaulted_GetRef();
					Disc.Name = FString::Printf(TEXT("%s clearing %s"), *Whose, *Clearing.Id.ToString());
					Disc.Form = EForm::Disc;
					Disc.Points = { VeyraLayout::ForTeam(VeyraLayout::ToVector(Clearing.Center), Team) };
					Disc.HalfWidths = { Clearing.Radius };
					Disc.Bounds = FBox2D(Disc.Points[0] - FVector2D(Clearing.Radius), Disc.Points[0] + FVector2D(Clearing.Radius));
				}
				for (const FVeyraRouteLayout& Route : Layout.Routes)
				{
					TArray<FVector2D> Path;
					for (const FVeyraMapPoint& Point : Route.Points)
					{
						Path.Add(VeyraLayout::ForTeam(VeyraLayout::ToVector(Point), Team));
					}
					AddBand(FString::Printf(TEXT("%s route %s"), *Whose, *Route.Id), Path, [&Route](int32) { return Route.Width / 2.0; });
				}
			}
		}

		/** How deep Point lies inside the deepest open space, and which that is; negative outside them all. */
		double Depth(const FVector2D& Point, const FString*& Deepest) const
		{
			double Most = -TNumericLimits<double>::Max();
			for (const FShape& Shape : Shapes)
			{
				if (!Shape.Bounds.IsInside(Point))
				{
					continue;
				}
				double Here = -TNumericLimits<double>::Max();
				switch (Shape.Form)
				{
				case EForm::Band:
					for (int32 Span = 0; Span + 1 < Shape.Points.Num(); ++Span)
					{
						const FVector2D Closest = FMath::ClosestPointOnSegment2D(Point, Shape.Points[Span], Shape.Points[Span + 1]);
						Here = FMath::Max(Here, Shape.HalfWidths[Span] - FVector2D::Distance(Point, Closest));
					}
					break;
				case EForm::Disc:
					Here = Shape.HalfWidths[0] - FVector2D::Distance(Point, Shape.Points[0]);
					break;
				case EForm::Polygon:
					Here = VeyraLayout::DepthInPolygon(Shape.Points, Point);
					break;
				}
				if (Here > Most)
				{
					Most = Here;
					Deepest = &Shape.Name;
				}
			}
			return Most;
		}

	private:
		enum class EForm : uint8
		{
			Band,
			Disc,
			Polygon,
		};

		struct FShape
		{
			FString Name;
			EForm Form = EForm::Band;
			/** A band's path, a disc's centre or a polygon's corners. */
			TArray<FVector2D> Points;
			/** A band's half width over each span, or a disc's radius. */
			TArray<double> HalfWidths;
			/** Where it can reach; a point outside lies outside it. */
			FBox2D Bounds = FBox2D(ForceInit);
		};

		template <typename FHalfWidthOf>
		void AddBand(const FString& Name, const TArray<FVector2D>& Path, FHalfWidthOf HalfWidthOf)
		{
			if (Path.Num() < 2)
			{
				return;
			}
			FShape& Band = Shapes.AddDefaulted_GetRef();
			Band.Name = Name;
			Band.Form = EForm::Band;
			Band.Points = Path;
			for (int32 Span = 0; Span + 1 < Path.Num(); ++Span)
			{
				const double Half = HalfWidthOf(Span);
				Band.HalfWidths.Add(Half);
				Band.Bounds += FBox2D(Path[Span] - FVector2D(Half), Path[Span] + FVector2D(Half));
				Band.Bounds += FBox2D(Path[Span + 1] - FVector2D(Half), Path[Span + 1] + FVector2D(Half));
			}
		}

		TArray<FShape> Shapes;
	};

	/**
	 * The open spaces (ADR-073 §1): Team A's keep with its walls and gates, the clearings and routes, each lying in the
	 * playable square. Team B's are their rotation.
	 */
	void ValidateSpaces(const FVeyraWorldTuning& Tuning, TArray<FString>& Problems)
	{
		const FVeyraBattlegroundLayout& Layout = Tuning.Layout;
		const double Playable = VeyraLayout::PlayableHalfExtent(Layout);
		const auto InPlayable = [Playable](const FVector2D& Point, double Reach) {
			return FMath::Abs(Point.X) + Reach <= Playable && FMath::Abs(Point.Y) + Reach <= Playable;
		};
		if (!(Playable > 0.0))
		{
			Problems.Add(TEXT("/layout/terrain/boundaryWidth: the rim's band must leave a playable square inside the floor"));
			return;
		}

		// The keep: a simple floor in Team A's half and the playable square, holding the base and its lanes' inhibitors.
		const FVeyraKeepLayout& Keep = Layout.Keep;
		const TArray<FVector2D> Floor = ToVectors(Keep.Floor);
		if (!IsSimple(Floor))
		{
			Problems.Add(TEXT("/layout/keep/floor: must be a simple polygon of three corners or more"));
		}
		else
		{
			if (!Algo::AllOf(Floor, [&](const FVector2D& Corner) { return InPlayable(Corner, 0.0) && VeyraLayout::DepthInTeamAHalf(Layout, Corner) > 0.0; }))
			{
				Problems.Add(TEXT("/layout/keep/floor: must lie in the playable square and wholly in Team A's half; Team B's is its rotation"));
			}
			for (const FVeyraStructurePlacement& Structure : VeyraLayout::Structures(Layout))
			{
				if (Structure.Team == EVeyraTeam::A && Structure.Kind != EVeyraStructureKind::LaneSpire
					&& VeyraLayout::DepthInPolygon(Floor, Structure.Location) < BodyRadius(Tuning, Structure.Kind))
				{
					Problems.Add(FString::Printf(TEXT("/layout/keep/floor: Team A's %s %s must stand wholly inside its keep"),
						*UEnum::GetValueAsString(Structure.Kind), Structure.Lane.IsSet() ? *UEnum::GetValueAsString(Structure.Lane.GetValue()) : TEXT("")));
				}
			}
			if (VeyraLayout::DepthInPolygon(Floor, VeyraLayout::ToVector(Layout.Base.Fountain)) < Layout.Base.FountainRadius)
			{
				Problems.Add(TEXT("/layout/base/fountain: the fountain must lie wholly inside the keep"));
			}
		}
		const TArray<FVeyraTerrainBox> KeepWalls = VeyraLayout::KeepWalls(Layout);
		for (int32 Index = 0; Index < Keep.Walls.Num(); ++Index)
		{
			const FVeyraKeepWallLayout& Run = Keep.Walls[Index];
			if (!IsPath(Run.Points) || !(Run.Thickness > 0.0) || !(Run.Height > 0.0) || !FMath::IsFinite(Run.Thickness) || !FMath::IsFinite(Run.Height))
			{
				Problems.Add(FString::Printf(TEXT("/layout/keep/walls/%d: a wall run needs two distinct points or more, a thickness and a height"), Index));
			}
		}
		for (const FVeyraLaneLayout& Lane : Layout.Lanes)
		{
			const TArray<FVector2D> Path = ToVectors(Lane.Points);
			const bool bBlocks = Algo::AnyOf(KeepWalls, [&](const FVeyraTerrainBox& Box) {
				for (int32 Span = 0; Span + 1 < Path.Num(); ++Span)
				{
					if (Box.DistanceToSegment(Path[Span], Path[Span + 1]) < Lane.Width / 2.0 - OutlineRounding)
					{
						return true;
					}
				}
				return false;
			});
			if (bBlocks)
			{
				Problems.Add(FString::Printf(TEXT("/layout/keep/walls: the keep's walls must stay clear of the %s lane's road"), *UEnum::GetValueAsString(Lane.Lane)));
			}
			const int32 Gates = Algo::CountIf(Keep.Gates, [&Lane](const FVeyraKeepGateLayout& Gate) { return Gate.Lane == Lane.Lane; });
			if (Gates != 1)
			{
				Problems.Add(FString::Printf(TEXT("/layout/keep/gates: the %s lane needs exactly one gate, not %d"), *UEnum::GetValueAsString(Lane.Lane), Gates));
			}
		}
		for (int32 Index = 0; Index < Keep.Gates.Num(); ++Index)
		{
			const FVeyraKeepGateLayout& Gate = Keep.Gates[Index];
			const FString Pointer = FString::Printf(TEXT("/layout/keep/gates/%d"), Index);
			const FVector2D Center = VeyraLayout::ToVector(Gate.Center);
			const FVeyraLaneLayout* Lane = Layout.Lanes.FindByPredicate([&Gate](const FVeyraLaneLayout& Each) { return Each.Lane == Gate.Lane; });
			if (!Lane || !FMath::IsFinite(Gate.Width) || Gate.Width < Lane->Width || VeyraLayout::DistanceToPath(Lane->Points, Center) > OutlineRounding)
			{
				Problems.Add(Pointer + TEXT(": a gate stands on its lane's path, at least as wide as its road"));
			}
			if (FMath::Abs(VeyraLayout::DepthInPolygon(Floor, Center)) > OutlineRounding)
			{
				Problems.Add(Pointer + TEXT("/center: a gate stands on the keep's edge"));
			}
			// A road's width clear through it, about its centre: the walls may turn away at its edges, but never across it.
			if (Lane && Algo::AnyOf(KeepWalls, [&](const FVeyraTerrainBox& Box) { return Box.DistanceTo(Center) < Lane->Width / 2.0 - OutlineRounding; }))
			{
				Problems.Add(Pointer + TEXT(": the keep's walls must leave the gate a road's width open"));
			}
		}

		// Clearings: a camp's ground is the clearing of its id.
		TSet<FVeyraContentId> ClearingIds;
		for (int32 Index = 0; Index < Layout.Clearings.Num(); ++Index)
		{
			const FVeyraClearingLayout& Clearing = Layout.Clearings[Index];
			const FString Pointer = FString::Printf(TEXT("/layout/clearings/%d"), Index);
			bool bAlreadyIn = false;
			ClearingIds.Add(Clearing.Id, &bAlreadyIn);
			if (!Clearing.Id.IsValid() || bAlreadyIn)
			{
				Problems.Add(Pointer + TEXT("/id: each clearing needs an id of its own"));
			}
			if (!(Clearing.Radius > 0.0) || !FMath::IsFinite(Clearing.Radius) || !InPlayable(VeyraLayout::ToVector(Clearing.Center), Clearing.Radius))
			{
				Problems.Add(Pointer + TEXT(": a clearing has a radius and lies wholly in the playable square"));
			}
			const bool bCamp = Algo::AnyOf(Tuning.Wildlife.Camps, [&Clearing](const FVeyraCampTuning& Camp) { return Camp.Id == Clearing.Id; });
			if (bCamp != (Clearing.Kind == EVeyraClearingKind::Camp))
			{
				Problems.Add(Pointer + TEXT("/kind: a clearing is a Camp exactly when a camp of its id stands on it"));
			}
		}

		// Routes: open paths between the spaces.
		TSet<FString> RouteIds;
		for (int32 Index = 0; Index < Layout.Routes.Num(); ++Index)
		{
			const FVeyraRouteLayout& Route = Layout.Routes[Index];
			const FString Pointer = FString::Printf(TEXT("/layout/routes/%d"), Index);
			bool bAlreadyIn = false;
			RouteIds.Add(Route.Id, &bAlreadyIn);
			if (Route.Id.IsEmpty() || bAlreadyIn)
			{
				Problems.Add(Pointer + TEXT("/id: each route needs an id of its own"));
			}
			if (!IsPath(Route.Points) || !(Route.Width > 0.0) || !FMath::IsFinite(Route.Width))
			{
				Problems.Add(Pointer + TEXT(": a route needs two distinct points or more and a width"));
			}
			else if (!Algo::AllOf(Route.Points, [&](const FVeyraMapPoint& Point) { return InPlayable(VeyraLayout::ToVector(Point), Route.Width / 2.0); }))
			{
				Problems.Add(Pointer + TEXT(": a route lies wholly in the playable square"));
			}
		}
		for (const FVeyraLaneLayout& Lane : Layout.Lanes)
		{
			if (!Algo::AllOf(Lane.Points, [&](const FVeyraMapPoint& Point) { return InPlayable(VeyraLayout::ToVector(Point), Lane.Width / 2.0 + Layout.Terrain.LaneShoulder); }))
			{
				Problems.Add(FString::Printf(TEXT("/layout/lanes: the %s lane's road and shoulders must lie in the playable square"), *UEnum::GetValueAsString(Lane.Lane)));
			}
		}
	}

	/**
	 * The masses (ADR-073 §2, §3): their rules, and the generated set itself, which is checked without being rebuilt. Each
	 * is a simple polygon, counter-clockwise, in the playable square; the set is its own half turn; and no outline runs
	 * into an open space beyond what its simplification allows.
	 */
	void ValidateMasses(const FVeyraBattlegroundLayout& Layout, TArray<FString>& Problems)
	{
		const FVeyraMassRules& Rules = Layout.MassRules;
		const double Positive[] = { Rules.MinThickness, Rules.MinArea, Rules.ForestMinArea, Rules.EdgeThickness, Rules.EdgeMaxLength, Rules.CornerRadius };
		const double NonNegative[] = { Rules.SimplifyTolerance, Rules.RiverMargin, Rules.VisualTolerance };
		if (!Algo::AllOf(Positive, [](double Value) { return FMath::IsFinite(Value) && Value > 0.0; })
			|| !Algo::AllOf(NonNegative, [](double Value) { return FMath::IsFinite(Value) && Value >= 0.0; }))
		{
			Problems.Add(TEXT("/layout/massRules: thickness, areas and edge boxes must be above 0, tolerances and the river's margin at least 0"));
			return;
		}
		// A mass's edge boxes face in from every side, so even its thinnest part keeps them apart.
		if (Rules.EdgeThickness * 2.0 > Rules.MinThickness)
		{
			Problems.Add(TEXT("/layout/massRules/edgeThickness: at most half minThickness, so the edges of a mass's two sides never cross"));
		}
		if (!(Layout.WallHalfHeight > 0.0))
		{
			Problems.Add(TEXT("/layout/wallHalfHeight: the terrain needs a height"));
		}

		const FVeyraMassesLayout& Masses = Layout.Masses;
		const bool bHashed = Masses.InputSha256.Len() == Sha256HexLength
			&& Algo::AllOf(Masses.InputSha256, [](TCHAR Character) { return FChar::IsDigit(Character) || (Character >= TEXT('a') && Character <= TEXT('f')); });
		if (!bHashed)
		{
			Problems.Add(TEXT("/layout/masses/inputSha256: must be the hash Game/Scripts/Crucible/BuildCrucibleMasses.py wrote"));
		}
		const double Playable = VeyraLayout::PlayableHalfExtent(Layout);
		const FOpenSpaces Open(Layout);
		const double Allowed = Rules.SimplifyTolerance + OutlineRounding;
		TSet<FString> Ids;
		const TArray<FVeyraMassShape> Shapes = VeyraLayout::Masses(Layout);
		for (const FVeyraMassShape& Shape : Shapes)
		{
			const FVeyraMassLayout& Mass = Masses.Items[Shape.Index];
			const FString Pointer = FString::Printf(TEXT("/layout/masses/items/%d"), Shape.Index);
			bool bAlreadyIn = false;
			Ids.Add(Mass.Id, &bAlreadyIn);
			if (Mass.Id.IsEmpty() || bAlreadyIn)
			{
				Problems.Add(Pointer + TEXT("/id: each mass needs an id of its own"));
			}
			if (!IsSimple(Shape.Outline) || DoubleArea(Shape.Outline) <= 0.0)
			{
				Problems.Add(Pointer + TEXT("/points: a mass is a simple polygon, its corners counter-clockwise"));
				continue;
			}
			// Each edge box lies flush along its span; at a corner turning further than a right angle it would stand out.
			const int32 Count = Shape.Outline.Num();
			bool bAcute = false;
			for (int32 Corner = 0; Corner < Count && !bAcute; ++Corner)
			{
				const FVector2D In = Shape.Outline[Corner] - Shape.Outline[(Corner + Count - 1) % Count];
				const FVector2D Out = Shape.Outline[(Corner + 1) % Count] - Shape.Outline[Corner];
				bAcute = FVector2D::CrossProduct(In, Out) > 0.0 && FVector2D::DotProduct(In, Out) < 0.0;
			}
			if (bAcute)
			{
				Problems.Add(Pointer + TEXT("/points: no corner of a mass may be sharper than a right angle, or its edge boxes stand out of it; rebuild the masses"));
			}
			if (!Algo::AllOf(Shape.Outline, [Playable](const FVector2D& Corner) { return FMath::Abs(Corner.X) <= Playable && FMath::Abs(Corner.Y) <= Playable; }))
			{
				Problems.Add(Pointer + TEXT("/points: a mass lies in the playable square"));
			}
			const bool bTurned = Algo::AnyOf(Shapes, [&Shape](const FVeyraMassShape& Other) {
				if (Other.Kind != Shape.Kind || Other.Outline.Num() != Shape.Outline.Num())
				{
					return false;
				}
				for (int32 Corner = 0; Corner < Shape.Outline.Num(); ++Corner)
				{
					if (!VeyraLayout::Rotate(Shape.Outline[Corner]).Equals(Other.Outline[Corner], OutlineRounding))
					{
						return false;
					}
				}
				return true;
			});
			if (!bTurned)
			{
				Problems.Add(Pointer + TEXT(": its half turn must be a mass of the same kind, so both teams have the same masses"));
			}
			// Along its outline, the deepest it runs into any open space.
			double Deepest = -TNumericLimits<double>::Max();
			const FString* Into = nullptr;
			FVector2D Where = FVector2D::ZeroVector;
			for (int32 Corner = 0; Corner < Shape.Outline.Num(); ++Corner)
			{
				const FVector2D& From = Shape.Outline[Corner];
				const FVector2D& To = Shape.Outline[(Corner + 1) % Shape.Outline.Num()];
				const int32 Steps = FMath::Max(1, FMath::CeilToInt32(FVector2D::Distance(From, To) / OutlineProbeStep));
				for (int32 Step = 0; Step < Steps; ++Step)
				{
					const FVector2D Point = FMath::Lerp(From, To, static_cast<double>(Step) / Steps);
					const FString* Space = nullptr;
					const double Depth = Open.Depth(Point, Space);
					if (Depth > Deepest)
					{
						Deepest = Depth;
						Into = Space;
						Where = Point;
					}
				}
			}
			if (Deepest > Allowed && Into)
			{
				Problems.Add(Pointer + FString::Printf(TEXT(": its outline runs %.0f into %s at (%.0f, %.0f), beyond simplifyTolerance: rebuild the masses"),
					Deepest, **Into, Where.X, Where.Y));
			}
		}
	}

	/** The ground's levels (ADR-040 §3): finite, inside the surface search, in order, and their nominal slopes walkable. */
	void ValidateTerrain(const FVeyraBattlegroundLayout& Layout, TArray<FString>& Problems)
	{
		const FVeyraTerrainTuning& Terrain = Layout.Terrain;
		const double Heights[] = { Terrain.LaneZ, Terrain.BaseZ, Terrain.JungleZ, Terrain.IslandZ, Terrain.BoundaryZ };
		const double Runs[] = { Terrain.JungleRise, Terrain.BankWidth, Terrain.BoundaryWidth };
		for (const double Height : Heights)
		{
			if (!FMath::IsFinite(Height) || Height <= Layout.Surface.MinZ || Height >= Layout.Surface.MaxZ)
			{
				Problems.Add(TEXT("/layout/terrain: every level must be finite and inside the surface search's bounds"));
				break;
			}
		}
		for (const double Run : Runs)
		{
			if (!FMath::IsFinite(Run) || Run <= 0.0)
			{
				Problems.Add(TEXT("/layout/terrain: every run must be finite and above 0"));
				break;
			}
		}
		if (!FMath::IsFinite(Terrain.LaneShoulder) || Terrain.LaneShoulder < 0.0 || !FMath::IsFinite(Terrain.WallFootingClearance) || Terrain.WallFootingClearance < 0.0)
		{
			Problems.Add(TEXT("/layout/terrain: laneShoulder and wallFootingClearance must be finite and at least 0"));
		}
		// The shore's shelf and a clearing's level core are shares of a run and a radius.
		if (!FMath::IsFinite(Terrain.UnderwaterShelfShare) || Terrain.UnderwaterShelfShare <= 0.0 || Terrain.UnderwaterShelfShare > 1.0
			|| !FMath::IsFinite(Terrain.ClearingCoreShare) || Terrain.ClearingCoreShare < 0.0 || !FMath::IsFinite(Terrain.ClearingFadeShare) || Terrain.ClearingFadeShare <= 0.0)
		{
			Problems.Add(TEXT("/layout/terrain: underwaterShelfShare must be above 0 and at most 1, clearingCoreShare at least 0 and clearingFadeShare above 0"));
		}
		if (Terrain.BoundaryZ <= Terrain.JungleZ || Terrain.IslandZ <= Layout.River.SurfaceZ)
		{
			Problems.Add(TEXT("/layout/terrain: the rim must rise above the jungle, and islands above the water"));
		}
		// The walkable climbs, at their nominal grades: road to jungle, road to base, water's edge to the highest bank.
		const auto Steepest = [&Layout](double Rise, double Run) { return Run > 0.0 && FMath::RadiansToDegrees(FMath::Atan(FMath::Abs(Rise) / Run)) >= Layout.Surface.MaxSlopeDegrees; };
		if (Steepest(Terrain.JungleZ - Terrain.LaneZ, Terrain.JungleRise) || Steepest(Terrain.BaseZ - Terrain.LaneZ, Terrain.JungleRise)
			|| Steepest(FMath::Max(Terrain.JungleZ, Terrain.LaneZ) - Layout.River.BedZ, Terrain.BankWidth))
		{
			Problems.Add(TEXT("/layout/terrain: a walkable climb (road to jungle or base, or a bank) is steeper than surface.maxSlopeDegrees"));
		}
	}

	/**
	 * The river (ADR-040 §6; author ruling 2026-10-05): its main channel from the centre off the floor, each island's
	 * channel leaving and rejoining it, every Flux Well dry on an island, and the sites each other's rotation.
	 */
	void ValidateRiver(const FVeyraWorldTuning& Tuning, TArray<FString>& Problems)
	{
		const FVeyraBattlegroundLayout& Layout = Tuning.Layout;
		const FVeyraRiverLayout& River = Layout.River;
		if (River.SamplesPerSegment < 1 || River.SamplesPerSegment > MaxRiverSamplesPerSegment)
		{
			Problems.Add(FString::Printf(TEXT("/layout/river/samplesPerSegment: must be 1 to %d"), MaxRiverSamplesPerSegment));
		}
		if (!FMath::IsFinite(River.SurfaceZ) || !FMath::IsFinite(River.BedZ) || River.BedZ >= River.SurfaceZ || River.BedZ <= Layout.Surface.MinZ
			|| River.SurfaceZ >= Layout.Terrain.LaneZ)
		{
			Problems.Add(TEXT("/layout/river: the bed must lie below the water's surface, inside the surface search, and the water below the lanes"));
		}
		if (!FMath::IsFinite(River.FlowSpeed) || River.FlowSpeed < 0.0)
		{
			Problems.Add(TEXT("/layout/river/flowSpeed: must be finite and at least 0"));
		}
		const auto ValidControls = [](TConstArrayView<FVeyraRiverPoint> Controls) {
			return Controls.Num() >= 2 && Algo::AllOf(Controls, [](const FVeyraRiverPoint& Control) {
				return FMath::IsFinite(Control.X) && FMath::IsFinite(Control.Y) && FMath::IsFinite(Control.Width) && Control.Width > 0.0;
			});
		};
		if (!ValidControls(River.Main))
		{
			Problems.Add(TEXT("/layout/river/main: needs at least two controls, finite, with widths above 0"));
			return;
		}
		if (River.Main[0].X != 0.0 || River.Main[0].Y != 0.0)
		{
			Problems.Add(TEXT("/layout/river/main/0: must be the centre, where Team A's half of the river meets its rotation"));
		}
		const FVeyraRiverPoint& Last = River.Main.Last();
		if (FMath::Max(FMath::Abs(Last.X), FMath::Abs(Last.Y)) <= Layout.HalfExtent)
		{
			Problems.Add(TEXT("/layout/river/main: must run off the floor"));
		}
		const FVeyraRiverShape& Shape = VeyraRiver::ShapeOf(Layout);
		// Its water is ground from the centre to walkableEnd each way; beyond, its gorges fall to the floor's edge (ADR-073 §1).
		if (!FMath::IsFinite(River.WalkableEnd) || River.WalkableEnd <= 0.0 || !Shape.HasGorges())
		{
			Problems.Add(TEXT("/layout/river/walkableEnd: must be above 0 and end before the main channel does, where its gorge begins"));
		}
		for (int32 Index = 0; Index < River.Islands.Num(); ++Index)
		{
			const FVeyraRiverIsland& Island = River.Islands[Index];
			const FString Pointer = FString::Printf(TEXT("/layout/river/islands/%d"), Index);
			if (!Tuning.FluxWells.Sites.IsValidIndex(Island.Site))
			{
				Problems.Add(Pointer + TEXT("/site: must name a Flux Well site"));
			}
			if (!ValidControls(Island.Channel))
			{
				Problems.Add(Pointer + TEXT("/channel: needs at least two controls, finite, with widths above 0"));
				continue;
			}
			const FVeyraRiverChannel& Main = Shape.GetChannels()[0];
			for (const FVeyraRiverPoint& End : { Island.Channel[0], Island.Channel.Last() })
			{
				if (FVeyraRiverShape::SignedDistance(Main, FVector2D(End.X, End.Y)) >= 0.0)
				{
					Problems.Add(Pointer + TEXT("/channel: must leave and rejoin the main channel, its ends in its water"));
					break;
				}
			}
		}
		const FVeyraFluxWellsTuning& Wells = Tuning.FluxWells;
		for (int32 Index = 0; Index < Wells.Sites.Num(); ++Index)
		{
			const FString Pointer = FString::Printf(TEXT("/fluxWells/sites/%d"), Index);
			const FVector2D Site = VeyraLayout::ToVector(Wells.Sites[Index]);
			if (Shape.SignedDistance(Site) < Wells.CapsuleRadius)
			{
				Problems.Add(Pointer + TEXT(": the Well's body must stand on dry ground"));
			}
			else if (!Shape.IsOnIsland(Site, Layout.HalfExtent * 2.0, IslandProbeDirections, IslandProbeStep))
			{
				Problems.Add(Pointer + TEXT(": the Well must stand on an island, the river closing around it"));
			}
			const bool bRotatesOntoASite = Algo::AnyOf(Wells.Sites, [&Site](const FVeyraMapPoint& Other) {
				return VeyraLayout::Rotate(Site).Equals(VeyraLayout::ToVector(Other), UE_DOUBLE_KINDA_SMALL_NUMBER);
			});
			if (!bRotatesOntoASite)
			{
				Problems.Add(Pointer + TEXT(": its rotation must be a site too, so both teams have the same Wells"));
			}
		}
	}
}

TArray<FString> Validate(const FVeyraWorldTuning& Tuning)
{
	TArray<FString> Problems;
	const FVeyraBattlegroundLayout& Layout = Tuning.Layout;
	if (!FMath::IsFinite(Layout.Surface.MinZ) || !FMath::IsFinite(Layout.Surface.MaxZ)
		|| Layout.Surface.MinZ >= Layout.Surface.MaxZ || !FMath::IsFinite(Layout.Surface.MaxSlopeDegrees)
		|| Layout.Surface.MaxSlopeDegrees < 0.0 || Layout.Surface.MaxSlopeDegrees >= 90.0)
	{
		Problems.Add(TEXT("/layout/surface: requires finite ascending height bounds and a slope in [0, 90) degrees"));
	}


	ValidateTerrain(Layout, Problems);
	ValidateRiver(Tuning, Problems);
	// Canon's three Fluxways, each once (Battleground Bible §2).
	for (const EVeyraLane Lane : { EVeyraLane::Top, EVeyraLane::Mid, EVeyraLane::Bottom })
	{
		const int32 Count = Layout.Lanes.FilterByPredicate([Lane](const FVeyraLaneLayout& Entry) { return Entry.Lane == Lane; }).Num();
		if (Count != 1)
		{
			Problems.Add(FString::Printf(TEXT("/layout/lanes: needs exactly one %s lane, not %d"), *UEnum::GetValueAsString(Lane), Count));
		}
	}

	for (int32 Index = 0; Index < Layout.Lanes.Num(); ++Index)
	{
		const FVeyraLaneLayout& Lane = Layout.Lanes[Index];
		const FString Pointer = FString::Printf(TEXT("/layout/lanes/%d"), Index);
		if (Lane.Points.Num() < 2)
		{
			Problems.Add(Pointer + TEXT("/points: a lane needs at least two points"));
			continue;
		}
		for (const FVeyraMapPoint& Point : Lane.Points)
		{
			if (!IsOnFloor(Point, Layout.HalfExtent))
			{
				Problems.Add(Pointer + TEXT("/points: every point must lie on the floor"));
				break;
			}
		}
		if (!VeyraLayout::RotatesOntoALane(Lane, Layout.Lanes))
		{
			Problems.Add(Pointer + TEXT("/points: the lane, turned half a turn about the centre and reversed, must be one of the lanes, so both teams walk the same distances"));
		}
		double Previous = 0.0;
		for (const double Distance : Lane.SpireDistances)
		{
			if (Distance <= Previous)
			{
				Problems.Add(Pointer + TEXT("/spireDistances: must be above 0 and strictly rising"));
				break;
			}
			Previous = Distance;
		}
		if (Lane.FluxbornSpawnDistance >= VeyraLayout::Length(Lane.Points) / 2.0)
		{
			Problems.Add(Pointer + TEXT("/fluxbornSpawnDistance: Team A's Fluxborn must spawn on its half of the lane"));
		}
		// The widest Fluxborn must fit between the lane's structures where they spawn, or it starts inside one.
		double WidestFluxborn = 0.0;
		for (const TPair<FVeyraContentId, FVeyraFluxbornDefinition>& Unit : Tuning.Fluxborn.Units)
		{
			WidestFluxborn = FMath::Max(WidestFluxborn, Unit.Value.CapsuleRadius);
		}
		const bool bClearOfInhibitor = FMath::Abs(Lane.FluxbornSpawnDistance - Lane.InhibitorDistance) > Tuning.Structures.Inhibitor.CapsuleRadius + WidestFluxborn;
		bool bClearOfSpires = true;
		for (const double Distance : Lane.SpireDistances)
		{
			bClearOfSpires &= FMath::Abs(Lane.FluxbornSpawnDistance - (Lane.InhibitorDistance + Distance)) > Tuning.Structures.LaneSpire.CapsuleRadius + WidestFluxborn;
		}
		if (!bClearOfInhibitor || !bClearOfSpires)
		{
			Problems.Add(Pointer + TEXT("/fluxbornSpawnDistance: Fluxborn would spawn inside the lane's inhibitor or a Spire"));
		}
		const double Outermost = Lane.InhibitorDistance + (Lane.SpireDistances.IsEmpty() ? 0.0 : Lane.SpireDistances.Last());
		if (Outermost >= VeyraLayout::Length(Lane.Points) / 2.0)
		{
			Problems.Add(Pointer + TEXT("/spireDistances: Team A's structures must stay on its half of the lane"));
		}
		// Each Spire beside the road on its side (author ruling 2026-10-12): on the shoulder, never on the road.
		if (Lane.SpireSides.Num() != Lane.SpireDistances.Num() || !Algo::AllOf(Lane.SpireSides, [](int32 Side) { return Side == 1 || Side == -1; }))
		{
			Problems.Add(Pointer + TEXT("/spireSides: one side, 1 or -1, for each Spire"));
		}
		if (!Lane.SpireDistances.IsEmpty()
			&& (Layout.SpireOffset <= Lane.Width / 2.0 || Layout.SpireOffset > Lane.Width / 2.0 + Layout.Terrain.LaneShoulder))
		{
			Problems.Add(Pointer + TEXT(": spireOffset must stand each Spire on the lane's shoulder, beside its road and not on it"));
		}
	}
	// The half turn carries Team A's top lane onto Team B's bottom one, keeping left as left: every structure's turn is its
	// counterpart's place only when the lanes it swaps list the same Spire sides.
	const TArray<FVeyraStructurePlacement> Placements = VeyraLayout::Structures(Layout);
	for (const FVeyraStructurePlacement& Placement : Placements)
	{
		const bool bTurned = Placement.Team != EVeyraTeam::A || Algo::AnyOf(Placements, [&Placement](const FVeyraStructurePlacement& Other) {
			return Other.Team == EVeyraTeam::B && Other.Kind == Placement.Kind && Other.Location.Equals(VeyraLayout::Rotate(Placement.Location), OutlineRounding);
		});
		if (!bTurned)
		{
			Problems.Add(FString::Printf(TEXT("/layout/lanes: Team A's %s at (%.0f, %.0f) turned half a turn must be Team B's; the lanes it swaps must list the same spireSides"),
				*UEnum::GetValueAsString(Placement.Kind), Placement.Location.X, Placement.Location.Y));
		}
	}

	const FVeyraBaseLayout& Base = Layout.Base;
	TArray<FVeyraMapPoint> BasePoints = Base.BaseTowers;
	BasePoints.Add(Base.PrimeWell);
	BasePoints.Add(Base.Fountain);
	for (const FVeyraMapPoint& Point : BasePoints)
	{
		if (!IsOnFloor(Point, Layout.HalfExtent))
		{
			Problems.Add(TEXT("/layout/base: every point must lie on the floor"));
			break;
		}
	}
	if (Base.BaseTowers.IsEmpty())
	{
		Problems.Add(TEXT("/layout/base/baseTowers: the Prime Well needs its base-defense towers (Battleground Bible §18)"));
	}
	// Each fog circle lies wholly on the floor and in Team A's half, so it and its rotation are apart: a
	// circle touching the dividing line could touch its rotation there, and touching circles are one
	// volume (ADR-016 §4).
	for (int32 Index = 0; Index < Layout.DenseFog.Num(); ++Index)
	{
		const FVeyraFogLayout& Circle = Layout.DenseFog[Index];
		const FVector2D Center = VeyraLayout::ToVector(Circle.Center);
		const FString Pointer = FString::Printf(TEXT("/layout/denseFog/%d"), Index);
		if (FMath::Abs(Center.X) + Circle.Radius > Layout.HalfExtent || FMath::Abs(Center.Y) + Circle.Radius > Layout.HalfExtent)
		{
			Problems.Add(Pointer + TEXT(": the fog must lie on the floor"));
		}
		if (VeyraLayout::DepthInTeamAHalf(Layout, Center) <= Circle.Radius)
		{
			Problems.Add(Pointer + TEXT(": the fog must lie wholly in Team A's half, clear of the dividing line; Team B's is its rotation"));
		}
		// Fog lines a lane's edges but never covers its road or shoulders (ADR-073 §4): the lanes stay open sight.
		for (const FVeyraLaneLayout& Lane : Layout.Lanes)
		{
			if (VeyraLayout::DistanceToPath(Lane.Points, Center) < Lane.Width / 2.0 + Layout.Terrain.LaneShoulder + Circle.Radius)
			{
				Problems.Add(Pointer + FString::Printf(TEXT(": the fog must stay off the %s lane's road and shoulders"), *UEnum::GetValueAsString(Lane.Lane)));
			}
		}
	}

	// Every Fluxborn attacks with the profile Vanguards use, so the same rules check it.
	if (Tuning.Fluxborn.Units.IsEmpty())
	{
		Problems.Add(TEXT("/fluxborn/units: needs at least one kind of Fluxborn"));
	}
	for (const TPair<FVeyraContentId, FVeyraFluxbornDefinition>& Unit : Tuning.Fluxborn.Units)
	{
		const FString Pointer = TEXT("/fluxborn/units/") + Unit.Key.ToString();
		for (const FString& AttackProblem : VeyraBasicAttacks::Validate(Unit.Value.BasicAttack))
		{
			Problems.Add(Pointer + TEXT("/basicAttack: ") + AttackProblem);
		}
		if (Unit.Value.CapsuleHalfHeight < Unit.Value.CapsuleRadius)
		{
			Problems.Add(Pointer + TEXT("/capsuleHalfHeight: must be at least capsuleRadius"));
		}
	}

	// Waves: phases in order from 0, and every unit a kind of Fluxborn (Battleground Bible §17).
	const FVeyraWavesTuning& Waves = Tuning.Waves;
	if (Waves.Phases.IsEmpty() || Waves.Phases[0].FromSeconds != 0.0)
	{
		Problems.Add(TEXT("/waves/phases: the first phase must begin at 0"));
	}
	for (int32 Index = 1; Index < Waves.Phases.Num(); ++Index)
	{
		if (Waves.Phases[Index].FromSeconds <= Waves.Phases[Index - 1].FromSeconds)
		{
			Problems.Add(FString::Printf(TEXT("/waves/phases/%d/fromSeconds: phases must begin in order"), Index));
		}
	}
	const auto CheckUnits = [&Problems, &Tuning](const TArray<FVeyraWaveUnitTuning>& Units, const TCHAR* Field) {
		for (int32 Index = 0; Index < Units.Num(); ++Index)
		{
			if (!Tuning.FindFluxborn(Units[Index].Unit))
			{
				Problems.Add(FString::Printf(TEXT("/waves/%s/%d/unit: %s is no kind of Fluxborn"), Field, Index, *Units[Index].Unit.ToString()));
			}
		}
	};
	CheckUnits(Waves.Units, TEXT("units"));
	CheckUnits(Waves.SiegeUnits, TEXT("siegeUnits"));
	CheckUnits(Waves.InhibitorDownUnits, TEXT("inhibitorDownUnits"));

	// Wildlife (Battleground Bible §7, §8, §17; ADR-014 §2).
	const FVeyraWildlifeTuning& Wildlife = Tuning.Wildlife;
	for (const TPair<FVeyraContentId, FVeyraWildlifeSpecies>& Species : Wildlife.Species)
	{
		const FString Pointer = TEXT("/wildlife/species/") + Species.Key.ToString();
		for (const FString& AttackProblem : VeyraBasicAttacks::Validate(Species.Value.BasicAttack))
		{
			Problems.Add(Pointer + TEXT("/basicAttack: ") + AttackProblem);
		}
		if (Species.Value.CapsuleHalfHeight < Species.Value.CapsuleRadius)
		{
			Problems.Add(Pointer + TEXT("/capsuleHalfHeight: must be at least capsuleRadius"));
		}
	}
	// Each of Team A's camps, its leash and all, on the floor, on Team A's half clear of the river's water, and
	// clear of every lane; Team B's are their rotation, so they are too. Each stands on the clearing of its id (ADR-073 §1).
	TSet<FVeyraContentId> CampIds;
	for (int32 Index = 0; Index < Wildlife.Camps.Num(); ++Index)
	{
		const FVeyraCampTuning& Camp = Wildlife.Camps[Index];
		const FString Pointer = FString::Printf(TEXT("/wildlife/camps/%d"), Index);
		const FVeyraWildlifeSpecies* Species = Tuning.FindSpecies(Camp.Species);
		if (!Species)
		{
			Problems.Add(Pointer + FString::Printf(TEXT("/species: %s is no species of wildlife"), *Camp.Species.ToString()));
		}
		bool bAlreadyIn = false;
		CampIds.Add(Camp.Id, &bAlreadyIn);
		if (!Camp.Id.IsValid() || bAlreadyIn)
		{
			Problems.Add(Pointer + TEXT("/id: each camp needs an id of its own"));
		}
		const FVeyraClearingLayout* Ground = Layout.Clearings.FindByPredicate([&Camp](const FVeyraClearingLayout& Clearing) { return Clearing.Id == Camp.Id; });
		if (!Ground || !VeyraLayout::ToVector(Ground->Center).Equals(VeyraLayout::ToVector(Camp.Center), OutlineRounding))
		{
			Problems.Add(Pointer + TEXT("/center: a camp stands at the centre of the clearing of its id"));
		}
		else if (Species && Camp.Spacing + Species->CapsuleRadius > Ground->Radius)
		{
			Problems.Add(Pointer + TEXT("/spacing: the camp's creatures must stand inside its clearing"));
		}
		const FVector2D Center = VeyraLayout::ToVector(Camp.Center);
		const double Reach = Camp.LeashRadius;
		if (FMath::Abs(Center.X) + Reach > Layout.HalfExtent || FMath::Abs(Center.Y) + Reach > Layout.HalfExtent)
		{
			Problems.Add(Pointer + TEXT("/center: the camp's leash must lie on the floor"));
		}
		if (VeyraLayout::DepthInTeamAHalf(Layout, Center) < Reach || VeyraRiver::ShapeOf(Layout).SignedDistance(Center) < Reach)
		{
			Problems.Add(Pointer + TEXT("/center: the camp must lie on Team A's half, its leash clear of the river's water"));
		}
		for (const FVeyraLaneLayout& Lane : Layout.Lanes)
		{
			if (VeyraLayout::DistanceToPath(Lane.Points, Center) < Lane.Width / 2.0 + Reach)
			{
				Problems.Add(Pointer + FString::Printf(TEXT("/center: the camp's leash must stay clear of the %s lane"), *UEnum::GetValueAsString(Lane.Lane)));
			}
		}
		if (Camp.Count > 1 && Camp.Spacing >= Reach)
		{
			Problems.Add(Pointer + TEXT("/spacing: the creatures must stand inside the camp's leash"));
		}
	}

	// The Flux Wells stand clear of every lane (§6; ADR-014 §4); ValidateRiver puts each on its island.
	const FVeyraFluxWellsTuning& Wells = Tuning.FluxWells;
	for (int32 Index = 0; Index < Wells.Sites.Num(); ++Index)
	{
		const FString Pointer = FString::Printf(TEXT("/fluxWells/sites/%d"), Index);
		const FVector2D Site = VeyraLayout::ToVector(Wells.Sites[Index]);
		if (!IsOnFloor(Wells.Sites[Index], Layout.HalfExtent))
		{
			Problems.Add(Pointer + TEXT(": must lie on the floor"));
		}
		for (const FVeyraLaneLayout& Lane : Layout.Lanes)
		{
			if (VeyraLayout::DistanceToPath(Lane.Points, Site) < Lane.Width / 2.0 + Wells.Radius)
			{
				Problems.Add(Pointer + FString::Printf(TEXT(": the Well's radius must stay clear of the %s lane"), *UEnum::GetValueAsString(Lane.Lane)));
			}
		}
	}
	if (Wells.CapsuleRadius >= Wells.Radius)
	{
		Problems.Add(TEXT("/fluxWells/radius: must reach beyond the Well's body"));
	}

	ValidateSpaces(Tuning, Problems);
	ValidateMasses(Layout, Problems);
	return Problems;
}
}
