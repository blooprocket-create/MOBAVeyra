// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraWorldValidateCommandlet.h"

#include "Algo/Transform.h"
#include "AssetCompilingManager.h"
#include "Battleground/VeyraBattlegroundTypes.h"
#include "Components/PrimitiveComponent.h"
#include "Dom/JsonObject.h"
#include "EditorWorldUtils.h"
#include "EngineUtils.h"
#include "ImageUtils.h"
#include "Interfaces/IPluginManager.h"
#include "Layout/VeyraLayout.h"
#include "Layout/VeyraRiver.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavAreas/NavArea.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Terrain/VeyraGround.h"
#include "Terrain/VeyraSurfacePlacement.h"
#include "Terrain/VeyraTerrainWall.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"
#include "VeyraBattlegroundSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogVeyraWorldValidate, Log, All);

namespace
{
	FString LaneName(EVeyraLane Lane)
	{
		return StaticEnum<EVeyraLane>()->GetNameStringByValue(static_cast<int64>(Lane));
	}

	/** An anchor's name (CrucibleValidation.json describes them) for one of Team A's structures. */
	FString NameOf(const FVeyraStructurePlacement& Placement)
	{
		switch (Placement.Kind)
		{
		case EVeyraStructureKind::LaneSpire:
			return FString::Printf(TEXT("Spire.%s.%d"), *LaneName(Placement.Lane.Get(EVeyraLane::Mid)), Placement.Order);
		case EVeyraStructureKind::Inhibitor:
			return TEXT("Inhibitor.") + LaneName(Placement.Lane.Get(EVeyraLane::Mid));
		case EVeyraStructureKind::BaseTower:
			return FString::Printf(TEXT("BaseTower.%d"), Placement.Order);
		default:
			return TEXT("PrimeWell");
		}
	}

	double Rounded(double Value, double Step)
	{
		return FMath::RoundToDouble(Value / Step) * Step;
	}

	/** How alike the two teams' measurements must be: the profile's provisional review values. */
	struct FTolerances
	{
		double RouteLengthFraction = 0.0;
		double RouteLengthUnits = 0.0;
		double ClimbUnits = 0.0;
		double HeightUnits = 0.0;
		double LaneDetourFraction = 0.0;
	};

	/** One team's walk along a route as the navigation finds it: its length, and the ground it climbs and descends. */
	struct FWalk
	{
		bool bComplete = false;
		double Length = 0.0;
		double Climb = 0.0;
		double Descent = 0.0;

		/** The steepest rise over run between two neighbouring ground samples. */
		double SteepestGrade = 0.0;

		/** The path's corners, for the navigation map. */
		TArray<FVector2D> Corners;

		TSharedRef<FJsonObject> ToJson() const
		{
			TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
			Out->SetBoolField(TEXT("complete"), bComplete);
			Out->SetNumberField(TEXT("length"), Rounded(Length, 1.0));
			Out->SetNumberField(TEXT("climb"), Rounded(Climb, 1.0));
			Out->SetNumberField(TEXT("descent"), Rounded(Descent, 1.0));
			Out->SetNumberField(TEXT("steepestGrade"), Rounded(SteepestGrade, 0.001));
			return Out;
		}
	};

	class FValidation
	{
	public:
		FValidation(UWorld& InWorld, const FVeyraWorldTuning& InTuning)
			: World(InWorld)
			, Tuning(InTuning)
			, Layout(InTuning.Layout)
			, River(VeyraRiver::ShapeOf(InTuning.Layout))
		{
		}

		bool Load(const FJsonObject& Profile, FString& Error)
		{
			const TSharedPtr<FJsonObject>* Limits = nullptr;
			if (!Profile.TryGetNumberField(TEXT("anchorReach"), Reach) || !Profile.TryGetNumberField(TEXT("groundSampleSpacing"), GroundSpacing)
				|| !Profile.TryGetNumberField(TEXT("terrainSampleSpacing"), TerrainSpacing) || !Profile.TryGetObjectField(TEXT("tolerances"), Limits)
				|| !(*Limits)->TryGetNumberField(TEXT("routeLengthFraction"), Tolerances.RouteLengthFraction)
				|| !(*Limits)->TryGetNumberField(TEXT("routeLengthUnits"), Tolerances.RouteLengthUnits)
				|| !(*Limits)->TryGetNumberField(TEXT("climbUnits"), Tolerances.ClimbUnits)
				|| !(*Limits)->TryGetNumberField(TEXT("heightUnits"), Tolerances.HeightUnits)
				|| !(*Limits)->TryGetNumberField(TEXT("laneDetourFraction"), Tolerances.LaneDetourFraction)
				|| Reach <= 0.0 || GroundSpacing <= 0.0 || TerrainSpacing <= 0.0)
			{
				Error = TEXT("CrucibleValidation.json needs a positive anchorReach, groundSampleSpacing and terrainSampleSpacing, and every tolerance.");
				return false;
			}

			// Team A's anchors, each by the one name a route uses.
			Anchors.Add(TEXT("Fountain"), VeyraLayout::Fountain(Layout, EVeyraTeam::A));
			for (const FVeyraStructurePlacement& Placement : VeyraLayout::Structures(Layout))
			{
				if (Placement.Team == EVeyraTeam::A)
				{
					Anchors.Add(NameOf(Placement), Placement.Location);
				}
			}
			for (const FVeyraCampTuning& Camp : Tuning.Wildlife.Camps)
			{
				Anchors.Add(TEXT("Camp.") + Camp.Species.ToString(), VeyraLayout::ToVector(Camp.Center));
			}
			for (int32 Site = 0; Site < Tuning.FluxWells.Sites.Num(); ++Site)
			{
				Anchors.Add(FString::Printf(TEXT("FluxWell.%d"), Site), VeyraLayout::ToVector(Tuning.FluxWells.Sites[Site]));
			}
			for (const FVeyraLaneLayout& Lane : Layout.Lanes)
			{
				Anchors.Add(TEXT("Crossing.") + LaneName(Lane.Lane), Crossing(Lane));
			}
			return true;
		}

		/** Generated presentation never blocks (World owns what blocks); the terrain blocks only as playable ground. */
		TSharedRef<FJsonObject> Collision()
		{
			int32 Presentation = 0;
			int32 Ground = 0;
			for (TActorIterator<AActor> It(&World); It; ++It)
			{
				const bool bPresentation = It->ActorHasTag(TEXT("Veyra.GeneratedDressing")) || It->ActorHasTag(TEXT("Veyra.RiverPresentation"));
				const bool bTerrain = It->ActorHasTag(TEXT("Veyra.AuthoredTerrain"));
				if (!bPresentation && !bTerrain)
				{
					continue;
				}
				It->ForEachComponent<UPrimitiveComponent>(/*bIncludeFromChildActors*/ false, [&](const UPrimitiveComponent* Component) {
					if (bPresentation)
					{
						++Presentation;
						if (Component->IsCollisionEnabled())
						{
							Findings.Add(FString::Printf(TEXT("Collision: %s on %s collides; generated presentation never does."), *Component->GetName(), *It->GetActorLabel()));
						}
					}
					else if (Component->IsCollisionEnabled())
					{
						++Ground;
						if (Component->GetCollisionObjectType() != VeyraGround::Channel)
						{
							Findings.Add(FString::Printf(TEXT("Collision: the terrain's %s is not on the ground channel."), *Component->GetName()));
						}
					}
				});
			}
			if (Ground == 0)
			{
				Findings.Add(TEXT("Collision: the terrain has no collision on the ground channel."));
			}
			TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
			Out->SetNumberField(TEXT("presentationComponents"), Presentation);
			Out->SetNumberField(TEXT("groundComponents"), Ground);
			return Out;
		}

		/**
		 * Every anchor stands on the playable surface, at the height its Team B rotation stands; and each of Team A's
		 * structures has its Team B counterpart at that rotation (the layout's protected geometry).
		 */
		TArray<TSharedPtr<FJsonValue>> Surfaces()
		{
			const TArray<FVeyraStructurePlacement> Structures = VeyraLayout::Structures(Layout);
			TArray<TSharedPtr<FJsonValue>> Out;
			for (const TPair<FString, FVector2D>& Anchor : Anchors)
			{
				const FVector2D Rotated = VeyraLayout::Rotate(Anchor.Value);
				FVector Mine;
				FVector Theirs;
				const bool bMine = VeyraSurfacePlacement::Resolve(World, Anchor.Value, 0.0, Layout.Surface, Mine);
				const bool bTheirs = VeyraSurfacePlacement::Resolve(World, Rotated, 0.0, Layout.Surface, Theirs);
				TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
				Entry->SetStringField(TEXT("anchor"), Anchor.Key);
				Entry->SetBoolField(TEXT("stands"), bMine && bTheirs);
				if (!bMine || !bTheirs)
				{
					Findings.Add(FString::Printf(TEXT("Surface: %s has no playable surface for %s."), *Anchor.Key, bMine ? TEXT("Team B") : TEXT("Team A")));
				}
				else
				{
					const double Delta = FMath::Abs(Mine.Z - Theirs.Z);
					Entry->SetNumberField(TEXT("teamA"), Rounded(Mine.Z, 0.1));
					Entry->SetNumberField(TEXT("teamB"), Rounded(Theirs.Z, 0.1));
					if (Delta > Tolerances.HeightUnits)
					{
						Findings.Add(FString::Printf(TEXT("Surface: %s stands %.1f higher for one team."), *Anchor.Key, Delta));
					}
				}
				Out.Add(MakeShared<FJsonValueObject>(Entry));
			}
			for (const FVeyraStructurePlacement& Placement : Structures)
			{
				const bool bCounterpart = Placement.Team != EVeyraTeam::A || Structures.ContainsByPredicate([&](const FVeyraStructurePlacement& Other) {
					return Other.Team == EVeyraTeam::B && Other.Kind == Placement.Kind && Other.Location.Equals(VeyraLayout::Rotate(Placement.Location), 1.0);
				});
				if (!bCounterpart)
				{
					Findings.Add(FString::Printf(TEXT("Layout: Team A's %s has no Team B counterpart at its rotation."), *NameOf(Placement)));
				}
			}
			return Out;
		}

		/** The playable ground at every sample of Team A's half and at its rotation: present at both, at one height. */
		TSharedRef<FJsonObject> Terrain()
		{
			int32 Samples = 0;
			int32 Missing = 0;
			int32 Uneven = 0;
			double Largest = 0.0;
			FVector2D Where = FVector2D::ZeroVector;
			const int32 Count = FMath::FloorToInt32(Layout.HalfExtent / TerrainSpacing);
			for (int32 X = -Count; X <= Count; ++X)
			{
				for (int32 Y = -Count; Y <= Count; ++Y)
				{
					const FVector2D Point(X * TerrainSpacing, Y * TerrainSpacing);
					if (VeyraLayout::DepthInTeamAHalf(Layout, Point) <= 0.0)
					{
						continue;
					}
					++Samples;
					double Mine = 0.0;
					double Theirs = 0.0;
					if (!Ground(Point, Mine) || !Ground(VeyraLayout::Rotate(Point), Theirs))
					{
						++Missing;
						continue;
					}
					const double Delta = FMath::Abs(Mine - Theirs);
					Uneven += Delta > Tolerances.HeightUnits ? 1 : 0;
					if (Delta > Largest)
					{
						Largest = Delta;
						Where = Point;
					}
				}
			}
			if (Missing > 0)
			{
				Findings.Add(FString::Printf(TEXT("Terrain: %d of %d samples lack playable ground for a team."), Missing, Samples));
			}
			if (Uneven > 0)
			{
				Findings.Add(FString::Printf(TEXT("Terrain: %d of %d samples differ in height between the teams, by up to %.1f at %s."), Uneven, Samples, Largest, *Where.ToString()));
			}
			TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
			Out->SetNumberField(TEXT("samples"), Samples);
			Out->SetNumberField(TEXT("missing"), Missing);
			Out->SetNumberField(TEXT("uneven"), Uneven);
			Out->SetNumberField(TEXT("largestDelta"), Rounded(Largest, 0.1));
			Out->SetStringField(TEXT("largestAt"), Where.ToString());
			return Out;
		}

		bool StartNavigation(TSharedRef<FJsonObject>& Out, FString& Error)
		{
			Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(&World);
			Mesh = Navigation ? Cast<ARecastNavMesh>(Navigation->GetDefaultNavDataInstance()) : nullptr;
			if (!Mesh)
			{
				Error = TEXT("The map built no navigation mesh.");
				return false;
			}
			Out->SetNumberField(TEXT("agentRadius"), Mesh->AgentRadius);
			Out->SetNumberField(TEXT("agentMaxSlope"), Mesh->AgentMaxSlope);
			return true;
		}

		/**
		 * Every wall blocks: no walkable navigation within the square inscribed in its thickness about its centre, which
		 * stays inside its footprint however it faces.
		 */
		TSharedRef<FJsonObject> Walls()
		{
			int32 Walkable = 0;
			const TArray<FVeyraTerrainBox> Boxes = VeyraLayout::Walls(Layout);
			for (const FVeyraTerrainBox& Box : Boxes)
			{
				double Z = 0.0;
				FNavLocation Location;
				const double Half = Box.Thickness / 2.0 * UE_INV_SQRT_2;
				// A wall is a dynamic obstacle: navigation marks its footprint with the obstacle area, which paths avoid.
				if (Ground(Box.Centre, Z)
					&& Navigation->ProjectPointToNavigation(FVector(Box.Centre, Z), Location, FVector(Half, Half, Layout.Surface.MaxZ - Layout.Surface.MinZ)))
				{
					const UClass* Area = Mesh->GetAreaClass(Mesh->GetPolyAreaID(Location.NodeRef));
					if (!Area || !Area->IsChildOf(FNavigationSystem::GetDefaultObstacleArea()))
					{
						++Walkable;
						Findings.Add(FString::Printf(TEXT("Navigation: the wall at %s can be walked through (%s)."), *Box.Centre.ToString(), Area ? *Area->GetName() : TEXT("no area")));
					}
				}
			}
			TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
			Out->SetNumberField(TEXT("walls"), Boxes.Num());
			Out->SetNumberField(TEXT("walkable"), Walkable);
			return Out;
		}
		/** The profile's routes: Team A's, and Team B's as their rotation. */
		TArray<TSharedPtr<FJsonValue>> Routes(const FJsonObject& Profile)
		{
			TArray<TSharedPtr<FJsonValue>> Out;
			for (const TSharedPtr<FJsonValue>& Value : Profile.GetArrayField(TEXT("routes")))
			{
				const TSharedPtr<FJsonObject> Route = Value->AsObject();
				FString FromName;
				FString ToName;
				const FVector2D* From = Route && Route->TryGetStringField(TEXT("from"), FromName) ? Anchors.Find(FromName) : nullptr;
				const FVector2D* To = Route && Route->TryGetStringField(TEXT("to"), ToName) ? Anchors.Find(ToName) : nullptr;
				if (!From || !To)
				{
					Findings.Add(FString::Printf(TEXT("Profile: a route names an unknown anchor (%s > %s)."), *FromName, *ToName));
					continue;
				}
				const FVector2D TeamA[] = { *From, *To };
				const FVector2D TeamB[] = { VeyraLayout::Rotate(*From), VeyraLayout::Rotate(*To) };
				const FWalk WalkA = Walk(TeamA);
				const FWalk WalkB = Walk(TeamB);
				Walked.Add({ WalkA.Corners, WalkB.Corners });
				Out.Add(MakeShared<FJsonValueObject>(Compare(FromName + TEXT(" > ") + ToName, WalkA, WalkB, {})));
			}
			return Out;
		}

		/** Each lane walked base to base along its waypoints, as its Fluxborn walk it: alike for both teams, near its drawn length. */
		TArray<TSharedPtr<FJsonValue>> Lanes()
		{
			TArray<TSharedPtr<FJsonValue>> Out;
			for (const FVeyraLaneLayout& Lane : Layout.Lanes)
			{
				const TArray<FVector2D> TeamA = VeyraLayout::Waypoints(Lane, EVeyraTeam::A);
				TArray<FVector2D> TeamB;
				Algo::Transform(TeamA, TeamB, [](const FVector2D& Point) { return VeyraLayout::Rotate(Point); });
				const FWalk WalkA = Walk(TeamA);
				const FWalk WalkB = Walk(TeamB);
				Walked.Add({ WalkA.Corners, WalkB.Corners });
				Out.Add(MakeShared<FJsonValueObject>(Compare(TEXT("Lane.") + LaneName(Lane.Lane), WalkA, WalkB, VeyraLayout::Length(Lane.Points))));
			}
			return Out;
		}

		/**
		 * The navigation as the server builds it, from above (World Validation Standard §13, navigation mode), with X to
		 * the right and Y up. Walkable ground is grey, lighter where higher; the obstacle area (the walls) is red; ground
		 * off the navigation is near black; the river is tinted blue. Each walked route is drawn over it, Team A's in blue
		 * and Team B's in orange.
		 */
		bool DrawMap(const FString& Filename, int32 Pixels, FString& Error) const
		{
			const double Span = Layout.HalfExtent * 2.0;
			const double Step = Span / Pixels;
			const double Depth = Layout.Surface.MaxZ - Layout.Surface.MinZ;
			const auto Blend = [](const FColor& From, const FColor& To, double Share) {
				const auto Channel = [Share](uint8 A, uint8 B) { return static_cast<uint8>(FMath::RoundToInt32(FMath::Lerp(static_cast<double>(A), static_cast<double>(B), Share))); };
				return FColor(Channel(From.R, To.R), Channel(From.G, To.G), Channel(From.B, To.B));
			};
			// Each pixel's walkable height (unset off the navigation, or in the obstacle area), shaded between the lowest
			// and highest walkable ground found, so the lanes, shelves and banks read.
			TArray<TOptional<double>> Walkable;
			TArray<bool> Obstacle;
			Walkable.SetNum(Pixels * Pixels);
			Obstacle.SetNumZeroed(Pixels * Pixels);
			FVector2D Heights(TNumericLimits<double>::Max(), TNumericLimits<double>::Lowest());
			for (int32 Row = 0; Row < Pixels; ++Row)
			{
				for (int32 Column = 0; Column < Pixels; ++Column)
				{
					const FVector2D Point(-Layout.HalfExtent + (Column + 0.5) * Step, Layout.HalfExtent - (Row + 0.5) * Step);
					double Z = 0.0;
					FNavLocation Location;
					if (Ground(Point, Z) && Navigation->ProjectPointToNavigation(FVector(Point, Z), Location, FVector(Step / 2.0, Step / 2.0, Depth))
						&& FVector2D::Distance(FVector2D(Location.Location), Point) <= Step)
					{
						const UClass* Area = Mesh->GetAreaClass(Mesh->GetPolyAreaID(Location.NodeRef));
						if (Area && Area->IsChildOf(FNavigationSystem::GetDefaultObstacleArea()))
						{
							Obstacle[Row * Pixels + Column] = true;
						}
						else
						{
							Walkable[Row * Pixels + Column] = Z;
							Heights = FVector2D(FMath::Min(Heights.X, Z), FMath::Max(Heights.Y, Z));
						}
					}
				}
			}
			const double Relief = FMath::Max(Heights.Y - Heights.X, UE_KINDA_SMALL_NUMBER);
			TArray<FColor> Image;
			Image.SetNumUninitialized(Pixels * Pixels);
			for (int32 Row = 0; Row < Pixels; ++Row)
			{
				for (int32 Column = 0; Column < Pixels; ++Column)
				{
					const int32 Index = Row * Pixels + Column;
					const FVector2D Point(-Layout.HalfExtent + (Column + 0.5) * Step, Layout.HalfExtent - (Row + 0.5) * Step);
					FColor Colour = Obstacle[Index] ? MapObstacle
						: Walkable[Index].IsSet() ? Blend(MapLow, MapHigh, (*Walkable[Index] - Heights.X) / Relief)
						: MapOffNavigation;
					if (River.SignedDistance(Point) < 0.0)
					{
						Colour = Blend(Colour, MapWater, MapWaterTint);
					}
					Image[Index] = Colour;
				}
			}
			const auto Plot = [&](TConstArrayView<FVector2D> Corners, const FColor& Colour) {
				for (int32 Index = 1; Index < Corners.Num(); ++Index)
				{
					const double Length = FVector2D::Distance(Corners[Index - 1], Corners[Index]);
					const int32 Steps = FMath::Max(1, FMath::CeilToInt32(Length / Step));
					for (int32 Each = 0; Each <= Steps; ++Each)
					{
						const FVector2D Point = FMath::Lerp(Corners[Index - 1], Corners[Index], static_cast<double>(Each) / Steps);
						const int32 Column = FMath::FloorToInt32((Point.X + Layout.HalfExtent) / Step);
						const int32 Row = FMath::FloorToInt32((Layout.HalfExtent - Point.Y) / Step);
						if (Column >= 0 && Column < Pixels && Row >= 0 && Row < Pixels)
						{
							Image[Row * Pixels + Column] = Colour;
						}
					}
				}
			};
			for (const TPair<TArray<FVector2D>, TArray<FVector2D>>& Route : Walked)
			{
				Plot(Route.Key, MapTeamA);
				Plot(Route.Value, MapTeamB);
			}
			if (!FImageUtils::SaveImageByExtension(*Filename, FImageView(Image.GetData(), Pixels, Pixels)))
			{
				Error = TEXT("Cannot write the navigation map ") + Filename;
				return false;
			}
			return true;
		}

		int32 CountWalls() const
		{
			int32 Walls = 0;
			for (TActorIterator<AVeyraTerrainWall> It(&World); It; ++It)
			{
				++Walls;
			}
			return Walls;
		}

		const TArray<FString>& GetFindings() const { return Findings; }

	private:
		bool Ground(const FVector2D& Point, double& OutZ) const
		{
			FHitResult Hit;
			if (!VeyraGround::Find(World, Point, Layout.Surface.MaxZ, Layout.Surface.MinZ, Hit))
			{
				return false;
			}
			OutZ = Hit.ImpactPoint.Z;
			return true;
		}

		/** Where the lane runs deepest into the river: its crossing. */
		FVector2D Crossing(const FVeyraLaneLayout& Lane) const
		{
			const double Length = VeyraLayout::Length(Lane.Points);
			FVector2D Deepest = VeyraLayout::PointAlong(Lane.Points, 0.0);
			double Least = River.SignedDistance(Deepest);
			for (double Along = GroundSpacing; Along <= Length; Along += GroundSpacing)
			{
				const FVector2D Point = VeyraLayout::PointAlong(Lane.Points, Along);
				const double Distance = River.SignedDistance(Point);
				if (Distance < Least)
				{
					Least = Distance;
					Deepest = Point;
				}
			}
			return Deepest;
		}

		/** The walkable place nearest an anchor: structures stand in their own navigation holes. */
		bool OnNavigation(const FVector2D& Point, FVector& Out) const
		{
			double Z = 0.0;
			FNavLocation Location;
			if (!Ground(Point, Z) || !Navigation->ProjectPointToNavigation(FVector(Point, Z), Location, FVector(Reach)))
			{
				return false;
			}
			Out = Location.Location;
			return true;
		}

		FWalk Walk(TConstArrayView<FVector2D> Stops) const
		{
			FWalk Out;
			Out.bComplete = Stops.Num() >= 2;
			TOptional<double> LastZ;
			for (int32 Stop = 1; Stop < Stops.Num() && Out.bComplete; ++Stop)
			{
				FVector From;
				FVector To;
				const UNavigationPath* Path = OnNavigation(Stops[Stop - 1], From) && OnNavigation(Stops[Stop], To)
					? UNavigationSystemV1::FindPathToLocationSynchronously(&World, From, To)
					: nullptr;
				if (!Path || !Path->IsValid() || Path->IsPartial())
				{
					Out.bComplete = false;
					break;
				}
				Out.Length += Path->GetPathLength();
				for (const FVector& Corner : Path->PathPoints)
				{
					Out.Corners.Add(FVector2D(Corner));
				}
				// The ground under the path, every ground sample spacing: what a unit walking it climbs and descends.
				for (int32 Corner = 1; Corner < Path->PathPoints.Num(); ++Corner)
				{
					const FVector2D A(Path->PathPoints[Corner - 1]);
					const FVector2D B(Path->PathPoints[Corner]);
					const double Span = FVector2D::Distance(A, B);
					if (Span <= UE_KINDA_SMALL_NUMBER)
					{
						continue;
					}
					const int32 Steps = FMath::Max(1, FMath::CeilToInt32(Span / GroundSpacing));
					for (int32 Step = LastZ.IsSet() ? 1 : 0; Step <= Steps; ++Step)
					{
						double Z = 0.0;
						if (!Ground(FMath::Lerp(A, B, static_cast<double>(Step) / Steps), Z))
						{
							continue;
						}
						if (LastZ.IsSet())
						{
							const double Rise = Z - *LastZ;
							(Rise > 0.0 ? Out.Climb : Out.Descent) += FMath::Abs(Rise);
							Out.SteepestGrade = FMath::Max(Out.SteepestGrade, FMath::Abs(Rise) / (Span / Steps));
						}
						LastZ = Z;
					}
				}
			}
			return Out;
		}

		TSharedRef<FJsonObject> Compare(const FString& Name, const FWalk& A, const FWalk& B, TOptional<double> Drawn)
		{
			TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
			Out->SetStringField(TEXT("route"), Name);
			Out->SetObjectField(TEXT("teamA"), A.ToJson());
			Out->SetObjectField(TEXT("teamB"), B.ToJson());
			TArray<FString> Failures;
			if (!A.bComplete || !B.bComplete)
			{
				Failures.Add(FString::Printf(TEXT("no complete path for %s"), !A.bComplete && !B.bComplete ? TEXT("either team") : A.bComplete ? TEXT("Team B") : TEXT("Team A")));
			}
			else
			{
				const double Delta = FMath::Abs(A.Length - B.Length);
				const double Allowed = FMath::Max(Tolerances.RouteLengthUnits, Tolerances.RouteLengthFraction * FMath::Max(A.Length, B.Length));
				Out->SetNumberField(TEXT("lengthDelta"), Rounded(Delta, 1.0));
				if (Delta > Allowed)
				{
					Failures.Add(FString::Printf(TEXT("lengths differ by %.0f (allowed %.0f)"), Delta, Allowed));
				}
				const double Relief = FMath::Max(FMath::Abs(A.Climb - B.Climb), FMath::Abs(A.Descent - B.Descent));
				Out->SetNumberField(TEXT("climbDelta"), Rounded(Relief, 1.0));
				if (Relief > Tolerances.ClimbUnits)
				{
					Failures.Add(FString::Printf(TEXT("climb or descent differs by %.0f (allowed %.0f)"), Relief, Tolerances.ClimbUnits));
				}
				if (Drawn.IsSet() && *Drawn > 0.0)
				{
					const double Detour = FMath::Max(A.Length, B.Length) / *Drawn - 1.0;
					Out->SetNumberField(TEXT("detour"), Rounded(Detour, 0.001));
					if (Detour > Tolerances.LaneDetourFraction)
					{
						Failures.Add(FString::Printf(TEXT("walks %.0f%% further than drawn (allowed %.0f%%)"), Detour * 100.0, Tolerances.LaneDetourFraction * 100.0));
					}
				}
			}
			Out->SetBoolField(TEXT("passed"), Failures.IsEmpty());
			for (const FString& Failure : Failures)
			{
				Findings.Add(FString::Printf(TEXT("Route %s: %s."), *Name, *Failure));
			}
			return Out;
		}

		// The navigation map's colours.
		static inline const FColor MapOffNavigation = FColor(22, 22, 26);
		static inline const FColor MapLow = FColor(70, 76, 70);
		static inline const FColor MapHigh = FColor(215, 222, 210);
		static inline const FColor MapObstacle = FColor(205, 55, 50);
		static inline const FColor MapWater = FColor(40, 120, 175);
		static constexpr double MapWaterTint = 0.45;
		static inline const FColor MapTeamA = FColor(60, 150, 255);
		static inline const FColor MapTeamB = FColor(255, 150, 40);

		UWorld& World;
		const FVeyraWorldTuning& Tuning;
		const FVeyraBattlegroundLayout& Layout;
		const FVeyraRiverShape& River;
		UNavigationSystemV1* Navigation = nullptr;
		const ARecastNavMesh* Mesh = nullptr;
		TMap<FString, FVector2D> Anchors;
		FTolerances Tolerances;
		double Reach = 0.0;
		double GroundSpacing = 0.0;
		double TerrainSpacing = 0.0;
		TArray<FString> Findings;

		/** Each route's walk for both teams, for the navigation map. */
		TArray<TPair<TArray<FVector2D>, TArray<FVector2D>>> Walked;
	};
}

UVeyraWorldValidateCommandlet::UVeyraWorldValidateCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UVeyraWorldValidateCommandlet::Main(const FString& /*Params*/)
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("VeyraWorldTools"));
	FString Json;
	TSharedPtr<FJsonObject> Profile;
	FString MapName;
	FString ReportPath;
	if (!Plugin || !FFileHelper::LoadFileToString(Json, *(Plugin->GetBaseDir() / TEXT("Config/CrucibleValidation.json")))
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Profile) || !Profile.IsValid()
		|| !Profile->TryGetStringField(TEXT("map"), MapName) || !Profile->TryGetStringField(TEXT("report"), ReportPath))
	{
		UE_LOG(LogVeyraWorldValidate, Error, TEXT("Cannot load CrucibleValidation.json, its map and its report path."));
		return 1;
	}

	UWorld::InitializationValues Values;
	Values.RequiresHitProxies(false).ShouldSimulatePhysics(false).EnableTraceCollision(true).CreateNavigation(true).CreateAISystem(false)
		.AllowAudioPlayback(false).CreatePhysicsScene(true);
	FScopedEditorWorld Scope(MapName, Values);
	UWorld* World = Scope.GetWorld();
	if (!World)
	{
		UE_LOG(LogVeyraWorldValidate, Error, TEXT("Cannot load %s."), *MapName);
		return 1;
	}

	const FVeyraWorldTuning& Tuning = UVeyraWorldTuningSubsystem::Get();
	FValidation Validation(*World, Tuning);
	FString Error;
	if (!Validation.Load(*Profile, Error))
	{
		UE_LOG(LogVeyraWorldValidate, Error, TEXT("%s"), *Error);
		return 1;
	}

	TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetStringField(TEXT("map"), MapName);
	Report->SetStringField(TEXT("generatedUtc"), FDateTime::UtcNow().ToIso8601());
	Report->SetObjectField(TEXT("collision"), Validation.Collision());
	Report->SetArrayField(TEXT("anchors"), Validation.Surfaces());
	Report->SetObjectField(TEXT("terrain"), Validation.Terrain());

	// The layout's walls, raised as the server raises them, then the navigation the server builds over them. The
	// structures stay out: each stands in its own navigation hole, and the layout check above places them.
	UVeyraBattlegroundSubsystem* Battleground = World->GetSubsystem<UVeyraBattlegroundSubsystem>();
	if (!Battleground)
	{
		UE_LOG(LogVeyraWorldValidate, Error, TEXT("The map's world has no battleground subsystem."));
		return 1;
	}
	Battleground->RaiseWalls(Tuning.Layout);
	// A wall takes its shape, collision and obstacle as play begins (AVeyraTerrainWall::BeginPlay); this world never
	// begins play, so begin theirs.
	for (TActorIterator<AVeyraTerrainWall> It(World); It; ++It)
	{
		It->DispatchBeginPlay();
	}
	FNavigationSystem::AddNavigationSystemToWorld(*World, FNavigationSystemRunMode::EditorMode);
	if (UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		// An editor world holds its navigation back until loading and asset compilation are done, which it learns by
		// ticking; a commandlet's world never ticks, so finish them here and release the hold.
		FAssetCompilingManager::Get().FinishAllCompilation();
		NavigationSystem->RemoveNavigationBuildLock(ENavigationBuildLock::AsyncLoadLock, UNavigationSystemV1::ELockRemovalRebuildAction::NoRebuild);
	}
	FNavigationSystem::Build(*World);
	TSharedRef<FJsonObject> Navigation = MakeShared<FJsonObject>();
	if (!Validation.StartNavigation(Navigation, Error))
	{
		UE_LOG(LogVeyraWorldValidate, Error, TEXT("%s"), *Error);
		return 1;
	}
	Navigation->SetNumberField(TEXT("walls"), Validation.CountWalls());
	Report->SetObjectField(TEXT("navigation"), Navigation);
	Report->SetObjectField(TEXT("walls"), Validation.Walls());
	Report->SetArrayField(TEXT("routes"), Validation.Routes(*Profile));
	Report->SetArrayField(TEXT("lanes"), Validation.Lanes());
	FString MapImage;
	double MapPixels = 0.0;
	if (!Profile->TryGetStringField(TEXT("navigationMap"), MapImage) || !Profile->TryGetNumberField(TEXT("navigationMapPixels"), MapPixels) || MapPixels < 1.0
		|| !Validation.DrawMap(FPaths::ProjectDir() / MapImage, static_cast<int32>(MapPixels), Error))
	{
		UE_LOG(LogVeyraWorldValidate, Error, TEXT("%s"), Error.IsEmpty() ? TEXT("CrucibleValidation.json needs navigationMap and navigationMapPixels.") : *Error);
		return 1;
	}
	Report->SetStringField(TEXT("navigationMap"), MapImage);

	TArray<TSharedPtr<FJsonValue>> Findings;
	for (const FString& Finding : Validation.GetFindings())
	{
		UE_LOG(LogVeyraWorldValidate, Display, TEXT("Finding: %s"), *Finding);
		Findings.Add(MakeShared<FJsonValueString>(Finding));
	}
	Report->SetBoolField(TEXT("passed"), Findings.IsEmpty());
	Report->SetArrayField(TEXT("findings"), Findings);

	FString Written;
	const FString Filename = FPaths::ProjectDir() / ReportPath;
	if (!FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Written)) || !FFileHelper::SaveStringToFile(Written, *Filename))
	{
		UE_LOG(LogVeyraWorldValidate, Error, TEXT("Cannot write %s."), *Filename);
		return 1;
	}
	UE_LOG(LogVeyraWorldValidate, Display, TEXT("%s: %d findings. Report: %s"), Findings.IsEmpty() ? TEXT("Passed") : TEXT("Failed"), Findings.Num(), *Filename);
	return Findings.IsEmpty() ? 0 : 1;
}
