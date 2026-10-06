// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Algo/Accumulate.h"
#include "CQTest.h"
#include "Engine/Engine.h"
#include "Layout/VeyraLayout.h"
#include "Layout/VeyraRiver.h"
#include "Tuning/VeyraFluxTuningSubsystem.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraWorldTests
{
	// Veyra.World.Layout.*: the committed layout loads, and its geometry gives both teams the same
	// battleground (ADR-011 §12).
	TEST_CLASS(Layout, "Veyra.World")
	{
		static constexpr double Tolerance = 1e-6;

		// The river is sampled from its two halves separately, so a point and its rotation agree to a fraction of a unit;
		// and how finely an island is probed. Fixture values.
		static constexpr double RiverSymmetryTolerance = 0.5;
		static constexpr double MeanderDegrees = 90.0;
		static constexpr int32 IslandDirections = 24;
		static constexpr double IslandStep = 100.0;

		static const FVeyraBattlegroundLayout& Committed()
		{
			return UVeyraWorldTuningSubsystem::Get().Layout;
		}

		/** A point in the committed river's main channel, well inside Team A's half where the river bends into it. */
		static FVeyraMapPoint RiverInTeamAsHalf()
		{
			const FVeyraBattlegroundLayout& Layout = Committed();
			const FVeyraRiverChannel& Main = VeyraRiver::ShapeOf(Layout).GetChannels()[0];
			const FVeyraRiverSample* Deepest = &Main.Samples[0];
			for (const FVeyraRiverSample& Sample : Main.Samples)
			{
				if (VeyraLayout::DepthInTeamAHalf(Layout, Sample.Point) > VeyraLayout::DepthInTeamAHalf(Layout, Deepest->Point))
				{
					Deepest = &Sample;
				}
			}
			return { Deepest->Point.X, Deepest->Point.Y };
		}

		TEST_METHOD(TheCommittedFileLoads)
		{
			ASSERT_THAT(IsTrue(GEngine->GetEngineSubsystem<UVeyraWorldTuningSubsystem>()->IsLoaded()));
			const TArray<FString> Problems = VeyraWorld::Validate(UVeyraWorldTuningSubsystem::Get());
			ASSERT_THAT(IsTrue(Problems.IsEmpty(), *FString::Join(Problems, TEXT(" | "))));
		}

		TEST_METHOD(TheRiverIsItsOwnRotationAndBends)
		{
			// Both teams share one river: the same at every point and its rotation (author ruling 2026-10-05).
			const FVeyraBattlegroundLayout& Layout = Committed();
			const FVeyraRiverShape& River = VeyraRiver::ShapeOf(Layout);
			ASSERT_THAT(IsTrue(River.IsWater(FVector2D::ZeroVector), TEXT("it runs through the centre")));
			for (double X = -Layout.HalfExtent; X <= Layout.HalfExtent; X += Layout.HalfExtent / 9.0)
			{
				for (double Y = -Layout.HalfExtent; Y <= Layout.HalfExtent; Y += Layout.HalfExtent / 9.0)
				{
					const FVector2D Point(X, Y);
					ASSERT_THAT(IsNear(River.SignedDistance(Point), River.SignedDistance(VeyraLayout::Rotate(Point)), RiverSymmetryTolerance));
				}
			}
			// A naturally curved river meanders: its centreline turns through more than a right angle in all, bending both
			// ways along its course.
			const TArray<FVeyraRiverSample>& Main = River.GetChannels()[0].Samples;
			double Turned = 0.0;
			int32 BendsLeft = 0;
			int32 BendsRight = 0;
			for (int32 Index = 2; Index < Main.Num(); ++Index)
			{
				const FVector2D Before = (Main[Index - 1].Point - Main[Index - 2].Point).GetSafeNormal();
				const FVector2D After = (Main[Index].Point - Main[Index - 1].Point).GetSafeNormal();
				const double Turn = FMath::Atan2(Before.X * After.Y - Before.Y * After.X, FVector2D::DotProduct(Before, After));
				Turned += FMath::Abs(Turn);
				BendsLeft += Turn > 0.0 ? 1 : 0;
				BendsRight += Turn < 0.0 ? 1 : 0;
			}
			ASSERT_THAT(IsTrue(FMath::RadiansToDegrees(Turned) > MeanderDegrees, TEXT("the river turns, not a straight line")));
			ASSERT_THAT(IsTrue(BendsLeft > 0 && BendsRight > 0, TEXT("it bends both ways")));
		}

		TEST_METHOD(EveryFluxWellStandsOnAnIsland)
		{
			const FVeyraWorldTuning& Tuning = UVeyraWorldTuningSubsystem::Get();
			const FVeyraRiverShape& River = VeyraRiver::ShapeOf(Tuning.Layout);
			for (const FVeyraMapPoint& Site : Tuning.FluxWells.Sites)
			{
				const FVector2D Point = VeyraLayout::ToVector(Site);
				ASSERT_THAT(IsTrue(River.SignedDistance(Point) > Tuning.FluxWells.CapsuleRadius, TEXT("the Well stands dry")));
				ASSERT_THAT(IsTrue(River.IsOnIsland(Point, Tuning.Layout.HalfExtent, IslandDirections, IslandStep), TEXT("the river closes around it")));
			}
			ASSERT_THAT(IsFalse(River.IsOnIsland(VeyraLayout::ToVector(Tuning.Layout.Base.PrimeWell), Tuning.Layout.HalfExtent, IslandDirections, IslandStep),
				TEXT("a base is not on an island")));
		}

		TEST_METHOD(ValidationRefusesARiverThatDoesNotJoinUp)
		{
			FVeyraWorldTuning Broken = UVeyraWorldTuningSubsystem::Get();
			ASSERT_THAT(IsFalse(Broken.Layout.River.Islands.IsEmpty()));
			Broken.Layout.River.Main[0].X = 100.0;
			// An island channel that wanders off into the jungle, and one for a site that does not exist.
			Broken.Layout.River.Islands[0].Channel.Last() = { -3000.0, -6000.0, 300.0 };
			Broken.Layout.River.Islands.Add({ Broken.FluxWells.Sites.Num(), Broken.Layout.River.Islands[0].Channel });
			const TArray<FString> Problems = VeyraWorld::Validate(Broken);
			const FString All = FString::Join(Problems, TEXT(" | "));
			const auto Mentions = [&Problems](const TCHAR* Text) { return Problems.ContainsByPredicate([Text](const FString& Problem) { return Problem.Contains(Text); }); };
			ASSERT_THAT(IsTrue(Mentions(TEXT("/layout/river/main/0: must be the centre")), All));
			ASSERT_THAT(IsTrue(Mentions(TEXT("/layout/river/islands/0/channel: must leave and rejoin the main channel")), All));
			ASSERT_THAT(IsTrue(Mentions(TEXT("/layout/river/islands/1/site: must name a Flux Well site")), All));
		}

		TEST_METHOD(TheRotationSwapsTheBasesAboutTheCentre)
		{
			const FVector2D Corner(-1000.0, -1000.0);
			ASSERT_THAT(IsTrue(VeyraLayout::Rotate(Corner).Equals(FVector2D(1000.0, 1000.0), Tolerance)));
			ASSERT_THAT(IsTrue(VeyraLayout::Rotate(FVector2D::ZeroVector).Equals(FVector2D::ZeroVector, Tolerance), TEXT("the centre is its own rotation")));
			ASSERT_THAT(IsTrue(VeyraLayout::Rotate(FVector2D(700.0, -700.0)).Equals(FVector2D(-700.0, 700.0), Tolerance)));
			ASSERT_THAT(IsTrue(VeyraLayout::Rotate(VeyraLayout::Rotate(Corner)).Equals(Corner, Tolerance)));
		}

		TEST_METHOD(DistancesAlongALaneFollowItsBends)
		{
			FVeyraLaneLayout Lane;
			Lane.Points = { { 0.0, 0.0 }, { 100.0, 0.0 }, { 100.0, 50.0 } };
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraLayout::Length(Lane.Points), 150.0, Tolerance)));
			ASSERT_THAT(IsTrue(VeyraLayout::PointAlong(Lane.Points, 40.0).Equals(FVector2D(40.0, 0.0), Tolerance)));
			ASSERT_THAT(IsTrue(VeyraLayout::PointAlong(Lane.Points, 120.0).Equals(FVector2D(100.0, 20.0), Tolerance)));
			ASSERT_THAT(IsTrue(VeyraLayout::PointAlong(Lane.Points, 1000.0).Equals(FVector2D(100.0, 50.0), Tolerance), TEXT("clamped to the end")));
			const TArray<FVector2D> TeamB = VeyraLayout::Waypoints(Lane, EVeyraTeam::B);
			ASSERT_THAT(IsTrue(TeamB.Num() == 3 && TeamB[0].Equals(FVector2D(100.0, 50.0), Tolerance), TEXT("Team B walks it the other way")));
		}

		TEST_METHOD(EachTeamHasEveryStructureAndTeamBsAreTeamAsRotated)
		{
			const FVeyraBattlegroundLayout& Layout = Committed();
			const TArray<FVeyraStructurePlacement> Placements = VeyraLayout::Structures(Layout);
			int32 SpiresPerTeam = 0;
			for (const FVeyraLaneLayout& Lane : Layout.Lanes)
			{
				SpiresPerTeam += Lane.SpireDistances.Num();
			}
			const int32 PerTeam = SpiresPerTeam + Layout.Lanes.Num() + Layout.Base.BaseTowers.Num() + 1;
			ASSERT_THAT(AreEqual(PerTeam * 2, Placements.Num()));

			const TArray<FVeyraStructurePlacement> TeamA = Placements.FilterByPredicate([](const FVeyraStructurePlacement& P) { return P.Team == EVeyraTeam::A; });
			const TArray<FVeyraStructurePlacement> TeamB = Placements.FilterByPredicate([](const FVeyraStructurePlacement& P) { return P.Team == EVeyraTeam::B; });
			ASSERT_THAT(AreEqual(TeamA.Num(), TeamB.Num()));
			for (int32 Index = 0; Index < TeamA.Num(); ++Index)
			{
				ASSERT_THAT(IsTrue(TeamA[Index].Kind == TeamB[Index].Kind && TeamA[Index].Lane == TeamB[Index].Lane && TeamA[Index].Order == TeamB[Index].Order));
				// Team A's structure, rotated, is one of Team B's of the same kind and order: on the lane its lane rotates onto.
				const FVector2D Rotated = VeyraLayout::Rotate(TeamA[Index].Location);
				ASSERT_THAT(IsTrue(TeamB.ContainsByPredicate([&](const FVeyraStructurePlacement& B) {
					return B.Kind == TeamA[Index].Kind && B.Order == TeamA[Index].Order && B.Location.Equals(Rotated, Tolerance);
				})));
			}
			// And each stands the same distance along its lane from its own team's end.
			for (const FVeyraLaneLayout& Lane : Layout.Lanes)
			{
				const FVector2D BEnd = VeyraLayout::ToVector(Lane.Points.Last());
				const FVeyraStructurePlacement* Inhibitor = TeamB.FindByPredicate([&Lane](const FVeyraStructurePlacement& B) { return B.Lane == Lane.Lane && B.Kind == EVeyraStructureKind::Inhibitor; });
				ASSERT_THAT(IsTrue(Inhibitor && FMath::IsNearlyEqual(FVector2D::Distance(Inhibitor->Location, BEnd), Lane.InhibitorDistance, Tolerance)));
			}
		}

		TEST_METHOD(EachTeamsWavesSpawnOnTheLaneTheyWalkBeforeTheirOwnInhibitor)
		{
			// Team B's spawn is its own end of the lane, never Team A's spawn rotated: the half turn carries the top lane
			// onto the bottom.
			const FVeyraBattlegroundLayout& Layout = Committed();
			const TArray<FVeyraStructurePlacement> Placements = VeyraLayout::Structures(Layout);
			const auto InhibitorOf = [&Placements](EVeyraTeam Team, EVeyraLane Lane) {
				return Placements.FindByPredicate([Team, Lane](const FVeyraStructurePlacement& P) {
					return P.Team == Team && P.Lane == Lane && P.Kind == EVeyraStructureKind::Inhibitor;
				});
			};
			for (const FVeyraLaneLayout& Lane : Layout.Lanes)
			{
				for (const EVeyraTeam Team : { EVeyraTeam::A, EVeyraTeam::B })
				{
					const FVector2D Spawn = VeyraLayout::FluxbornSpawnPoint(Lane, Team);
					ASSERT_THAT(IsTrue(VeyraLayout::DistanceToPath(Lane.Points, Spawn) < Tolerance, TEXT("on the lane it walks")));
					ASSERT_THAT(IsTrue(FVector2D::Distance(Spawn, VeyraLayout::Waypoints(Lane, Team)[0]) <= Lane.FluxbornSpawnDistance + Tolerance,
						TEXT("by the end it walks from")));
					const FVeyraStructurePlacement* Own = InhibitorOf(Team, Lane.Lane);
					const FVeyraStructurePlacement* Enemy = InhibitorOf(VeyraTeams::Opposing(Team), Lane.Lane);
					ASSERT_THAT(IsTrue(Own && Enemy && FVector2D::Distance(Spawn, Own->Location) < FVector2D::Distance(Spawn, Enemy->Location),
						TEXT("before its own inhibitor")));
				}
			}
		}

		TEST_METHOD(ALanesStructuresFallFromTheOuterSpireInward)
		{
			const FVeyraBattlegroundLayout& Layout = Committed();
			const FVeyraLaneLayout& Lane = Layout.Lanes[0];
			const FVector2D Start = VeyraLayout::ToVector(Lane.Points[0]);
			const TArray<FVeyraStructurePlacement> OnLane = VeyraLayout::Structures(Layout).FilterByPredicate([&Lane](const FVeyraStructurePlacement& P) {
				return P.Team == EVeyraTeam::A && P.Lane == Lane.Lane;
			});
			ASSERT_THAT(AreEqual(Lane.SpireDistances.Num() + 1, OnLane.Num()));
			for (int32 Index = 1; Index < OnLane.Num(); ++Index)
			{
				ASSERT_THAT(IsTrue(OnLane[Index].Order == OnLane[Index - 1].Order + 1));
			}
			ASSERT_THAT(IsTrue(OnLane.Last().Kind == EVeyraStructureKind::Inhibitor, TEXT("the inhibitor falls last")));
			ASSERT_THAT(IsTrue(FVector2D::Distance(OnLane[0].Location, Start) > FVector2D::Distance(OnLane[1].Location, Start),
				TEXT("the outer Spire stands farthest from the base")));
		}

		TEST_METHOD(ValidationCatchesWhatTheSchemaCannot)
		{
			FVeyraWorldTuning Broken = UVeyraWorldTuningSubsystem::Get();
			Broken.Layout.Lanes[0].Lane = Broken.Layout.Lanes[1].Lane;
			Broken.Layout.Lanes[1].Points[0].X += 1.0;
			Broken.Layout.Lanes[2].SpireDistances.Last() = VeyraLayout::Length(Broken.Layout.Lanes[2].Points);
			Broken.Layout.Base.Fountain.X = Broken.Layout.HalfExtent * 2.0;
			const TArray<FString> Problems = VeyraWorld::Validate(Broken);
			const FString All = FString::Join(Problems, TEXT(" | "));
			const auto Mentions = [&Problems](const TCHAR* Text) { return Problems.ContainsByPredicate([Text](const FString& Problem) { return Problem.Contains(Text); }); };
			ASSERT_THAT(IsTrue(Mentions(TEXT("needs exactly one")), All));
			ASSERT_THAT(IsTrue(Mentions(TEXT("/layout/lanes/1/points: the lane, turned half a turn about the centre and reversed, must be one of the lanes")), All));
			ASSERT_THAT(IsTrue(Mentions(TEXT("/layout/lanes/2/spireDistances: Team A's structures must stay on its half")), All));
			ASSERT_THAT(IsTrue(Mentions(TEXT("/layout/base: every point must lie on the floor")), All));
		}

		TEST_METHOD(DistancesToAPathAndIntoTeamAsHalf)
		{
			const TArray<FVeyraMapPoint> Path = { { 0.0, 0.0 }, { 100.0, 0.0 }, { 100.0, 100.0 } };
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraLayout::DistanceToPath(Path, FVector2D(50.0, 30.0)), 30.0, Tolerance)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraLayout::DistanceToPath(Path, FVector2D(130.0, 50.0)), 30.0, Tolerance), TEXT("the nearest segment")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraLayout::DistanceToPath(Path, FVector2D(-40.0, 0.0)), 40.0, Tolerance), TEXT("beyond an end")));
			// Team A's base is at negative X and Y in the committed layout.
			const FVeyraBattlegroundLayout& Layout = Committed();
			ASSERT_THAT(IsTrue(VeyraLayout::DepthInTeamAHalf(Layout, FVector2D(-100.0, -100.0)) > 0.0));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraLayout::DepthInTeamAHalf(Layout, FVector2D(300.0, -300.0)), 0.0, Tolerance), TEXT("on the line between the halves")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraLayout::DepthInTeamAHalf(Layout, FVector2D(100.0, 100.0)), -200.0 / UE_SQRT_2, Tolerance)));
		}

		TEST_METHOD(CampsStayOnTeamAsHalfOffTheLanesAndWellsOnIslands)
		{
			FVeyraWorldTuning Broken = UVeyraWorldTuningSubsystem::Get();
			ASSERT_THAT(IsTrue(Broken.Wildlife.Camps.Num() >= 3 && !Broken.FluxWells.Sites.IsEmpty()));
			// On Team B's half; on the mid lane; of no species.
			Broken.Wildlife.Camps[0].Center = { 3000.0, 3000.0 };
			Broken.Wildlife.Camps[1].Center = { -2000.0, -2000.0 };
			Broken.Wildlife.Camps[2].Species = FVeyraContentId::FromText(TEXT("unicorn")).GetValue();
			// Dry jungle in Team A's half, with no river around it.
			Broken.FluxWells.Sites[0] = { -1000.0, -3000.0 };
			const TArray<FString> Problems = VeyraWorld::Validate(Broken);
			const FString All = FString::Join(Problems, TEXT(" | "));
			const auto Mentions = [&Problems](const TCHAR* Text) { return Problems.ContainsByPredicate([Text](const FString& Problem) { return Problem.Contains(Text); }); };
			ASSERT_THAT(IsTrue(Mentions(TEXT("/wildlife/camps/0/center: the camp must lie on Team A's half")), All));
			ASSERT_THAT(IsTrue(Mentions(TEXT("/wildlife/camps/1/center: the camp's leash must stay clear of the EVeyraLane::Mid lane")), All));
			ASSERT_THAT(IsTrue(Mentions(TEXT("/wildlife/camps/2/species: unicorn is no species")), All));
			ASSERT_THAT(IsTrue(Mentions(TEXT("/fluxWells/sites/0: the Well must stand on an island")), All));
			ASSERT_THAT(IsTrue(Mentions(TEXT("/fluxWells/sites/0: its rotation must be a site too")), All));
		}

		TEST_METHOD(WallsLieInTeamAsHalfAndTeamBsAreTheirRotation)
		{
			// The battleground's walls (ADR-043 §1): Team A's, then their rotations, sample for sample and box for box.
			const FVeyraBattlegroundLayout& Layout = Committed();
			ASSERT_THAT(IsFalse(Layout.Walls.IsEmpty()));
			const TArray<FVeyraWallShape> Shapes = VeyraLayout::WallShapes(Layout);
			ASSERT_THAT(AreEqual(Shapes.Num(), Layout.Walls.Num() * 2));
			for (int32 Index = 0; Index < Layout.Walls.Num(); ++Index)
			{
				const FVeyraWallShape& A = Shapes[Index];
				const FVeyraWallShape& B = Shapes[Index + Layout.Walls.Num()];
				ASSERT_THAT(IsTrue(A.Team == EVeyraTeam::A && B.Team == EVeyraTeam::B && A.Index == Index && B.Index == Index));
				ASSERT_THAT(IsTrue(A.Spine.Num() > 1 && A.Spine.Num() == B.Spine.Num()));
				for (int32 Sample = 0; Sample < A.Spine.Num(); ++Sample)
				{
					ASSERT_THAT(IsTrue(B.Spine[Sample].Point.Equals(VeyraLayout::Rotate(A.Spine[Sample].Point), Tolerance) && B.Spine[Sample].Width == A.Spine[Sample].Width));
				}
				const TArray<FVeyraTerrainBox> BoxesA = A.Boxes();
				const TArray<FVeyraTerrainBox> BoxesB = B.Boxes();
				ASSERT_THAT(AreEqual(BoxesA.Num(), BoxesB.Num()));
				for (int32 Box = 0; Box < BoxesA.Num(); ++Box)
				{
					for (const FVector2D& Corner : BoxesA[Box].Corners())
					{
						ASSERT_THAT(IsTrue(VeyraLayout::DepthInTeamAHalf(Layout, Corner) > 0.0));
					}
					ASSERT_THAT(IsTrue(BoxesB[Box].Centre.Equals(VeyraLayout::Rotate(BoxesA[Box].Centre), Tolerance)));
				}
			}
			ASSERT_THAT(AreEqual(VeyraLayout::Walls(Layout).Num(), Algo::TransformAccumulate(Shapes, [](const FVeyraWallShape& Shape) { return Shape.Boxes().Num(); }, 0)));
		}

		TEST_METHOD(AWallsBoxesHoldItsRidgeAndCloseItsBends)
		{
			// A wall that bends and narrows: whatever lies within its ridge lies in one of its boxes, so nothing stands or
			// sees through the cliff the terrain raises, and no box reaches past the ridge by more than a square end holding
			// a rounded tip does.
			FVeyraBattlegroundLayout Layout;
			Layout.WallSamplesPerSegment = 4;
			Layout.Walls = { { TEXT("Test"), { { -2000.0, 0.0, 300.0 }, { -1000.0, 400.0, 800.0 }, { 0.0, 0.0, 600.0 }, { 600.0, -700.0, 250.0 } } } };
			const FVeyraWallShape Shape = VeyraLayout::WallShape(Layout, 0, EVeyraTeam::A);
			const TArray<FVeyraTerrainBox> Boxes = Shape.Boxes();
			ASSERT_THAT(AreEqual(Boxes.Num(), (Layout.Walls[0].Points.Num() - 1) * Layout.WallSamplesPerSegment));
			double Thickest = 0.0;
			for (const FVeyraCurveSample& Sample : Shape.Spine)
			{
				Thickest = FMath::Max(Thickest, Sample.Width);
			}
			const FBox2D Around(FVector2D(-2600.0, -1300.0), FVector2D(1200.0, 1000.0));
			for (double X = Around.Min.X; X <= Around.Max.X; X += 37.0)
			{
				for (double Y = Around.Min.Y; Y <= Around.Max.Y; Y += 37.0)
				{
					const FVector2D Point(X, Y);
					if (Shape.DepthInside(Point) > 0.0)
					{
						ASSERT_THAT(IsTrue(Boxes.ContainsByPredicate([&Point](const FVeyraTerrainBox& Box) { return Box.DistanceTo(Point) == 0.0; }),
							*FString::Printf(TEXT("inside the ridge at %s but in no box"), *Point.ToString())));
					}
				}
			}
			const double SquareEnd = (UE_DOUBLE_SQRT_2 - 1.0) * Thickest / 2.0;
			for (const FVeyraTerrainBox& Box : Boxes)
			{
				for (const FVector2D& Corner : Box.Corners())
				{
					ASSERT_THAT(IsTrue(Shape.DepthInside(Corner) >= -SquareEnd - Tolerance, *FString::Printf(TEXT("a box's corner at %s strays from the ridge"), *Corner.ToString())));
				}
			}
		}

		TEST_METHOD(AWallKeepsClearOfWhatStandsOnTheBattleground)
		{
			FVeyraWorldTuning Broken = UVeyraWorldTuningSubsystem::Get();
			const FVeyraMapPoint Camp = Broken.Wildlife.Camps[0].Center;
			const double Edge = Broken.Layout.HalfExtent;
			const FVeyraMapPoint River = RiverInTeamAsHalf();
			const auto Straight = [](const FVector2D& Centre, double Width) {
				return FVeyraWallLayout{ TEXT("Test"), { { Centre.X - 150.0, Centre.Y, Width }, { Centre.X + 150.0, Centre.Y, Width } } };
			};
			Broken.Layout.Walls = {
				Straight({ 0.0, 0.0 }, 100.0),                 // across the dividing line
				Straight({ -2000.0, -2000.0 }, 100.0),         // on the mid lane
				Straight({ Camp.X, Camp.Y }, 100.0),           // on a camp
				Straight({ -Edge, -1000.0 }, 100.0),           // off the floor's edge
				{ TEXT("Test"), { { -3000.0, -6000.0, 0.0 }, { -2700.0, -6000.0, 0.0 } } }, // no width
				Straight({ River.X, River.Y }, 100.0),         // in the river's water
				{ TEXT("Test"), { { -3000.0, -4500.0, 100.0 }, { -2600.0, -4500.0, 100.0 }, { -3000.0, -4450.0, 100.0 } } }, // doubling back
			};
			Broken.Layout.WallHalfHeight = 0.0;
			const TArray<FString> Problems = VeyraWorld::Validate(Broken);
			const FString All = FString::Join(Problems, TEXT(" | "));
			const auto Mentions = [&Problems](const TCHAR* Text) { return Problems.ContainsByPredicate([Text](const FString& Problem) { return Problem.Contains(Text); }); };
			ASSERT_THAT(IsTrue(Mentions(TEXT("/layout/wallHalfHeight: walls need a height")), All));
			ASSERT_THAT(IsTrue(Mentions(TEXT("/layout/walls/0: the wall must lie wholly in Team A's half")), All));
			ASSERT_THAT(IsTrue(Mentions(TEXT("/layout/walls/1: the wall must keep wallClearance from the EVeyraLane::Mid lane's road")), All));
			ASSERT_THAT(IsTrue(Mentions(*FString::Printf(TEXT("/layout/walls/2: the wall must keep wallClearance from what stands at (%.0f, %.0f)"), Camp.X, Camp.Y)), All));
			ASSERT_THAT(IsTrue(Mentions(TEXT("/layout/walls/3: the wall must lie on the floor")), All));
			ASSERT_THAT(IsTrue(Mentions(TEXT("/layout/walls/4: a wall needs at least two distinct points, each with a width above 0")), All));
			ASSERT_THAT(IsTrue(Mentions(TEXT("/layout/walls/5: the wall must keep wallClearance from the river's water")), All));
			ASSERT_THAT(IsTrue(Mentions(TEXT("/layout/walls/6: the wall's curve must turn less than a right angle between samples")), All));

			FVeyraWorldTuning Unsampled = UVeyraWorldTuningSubsystem::Get();
			Unsampled.Layout.WallSamplesPerSegment = 0;
			ASSERT_THAT(IsTrue(VeyraWorld::Validate(Unsampled).Contains(TEXT("/layout/wallSamplesPerSegment: walls need at least one sample a span"))));
		}
		TEST_METHOD(DenseFogLiesInTeamAsHalfAndTeamBsIsItsRotation)
		{
			// The battleground's bush (Battleground Bible §11): Team A's circles, then their rotations.
			const FVeyraBattlegroundLayout& Layout = UVeyraWorldTuningSubsystem::Get().Layout;
			ASSERT_THAT(IsFalse(Layout.DenseFog.IsEmpty()));
			const TArray<FVeyraFogPlacement> Fog = VeyraLayout::DenseFog(Layout);
			ASSERT_THAT(AreEqual(Fog.Num(), Layout.DenseFog.Num() * 2));
			for (int32 Index = 0; Index < Layout.DenseFog.Num(); ++Index)
			{
				const FVeyraFogPlacement& A = Fog[Index];
				const FVeyraFogPlacement& B = Fog[Index + Layout.DenseFog.Num()];
				ASSERT_THAT(IsTrue(VeyraLayout::DepthInTeamAHalf(Layout, A.Center) > A.Radius));
				ASSERT_THAT(IsTrue(B.Center.Equals(VeyraLayout::Rotate(A.Center)) && B.Radius == A.Radius));
			}

			// On Team B's half, off the floor, or touching the dividing line (and so perhaps its rotation) is refused.
			FVeyraWorldTuning Broken = UVeyraWorldTuningSubsystem::Get();
			Broken.Layout.DenseFog[0].Center = { 2000.0, 2000.0 };
			Broken.Layout.DenseFog[1].Center = { -Broken.Layout.HalfExtent, -3000.0 };
			FVeyraFogLayout& Tangent = Broken.Layout.DenseFog[2];
			Tangent.Radius = VeyraLayout::DepthInTeamAHalf(Broken.Layout, VeyraLayout::ToVector(Tangent.Center));
			const FString All = FString::Join(VeyraWorld::Validate(Broken), TEXT(" | "));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/layout/denseFog/0: the fog must lie wholly in Team A's half")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/layout/denseFog/1: the fog must lie on the floor")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/layout/denseFog/2: the fog must lie wholly in Team A's half")), All));
		}
	};

	// Veyra.Flux.FluxTuning.*: the committed Flux.json loads, and a grant's duration matches its kind.
	TEST_CLASS(FluxTuning, "Veyra.Flux")
	{
		TEST_METHOD(TheCommittedFileLoads)
		{
			ASSERT_THAT(IsTrue(GEngine->GetEngineSubsystem<UVeyraFluxTuningSubsystem>()->IsLoaded()));
		}

		TEST_METHOD(APermanentGrantHasNoDurationAndATemporaryOneDoes)
		{
			FVeyraFluxTuning Broken = UVeyraFluxTuningSubsystem::Get();
			Broken.Grants.LaneSpire.DurationSeconds = 1.0;
			Broken.Grants.Inhibitor.DurationSeconds = 0.0;
			const TArray<FString> Problems = VeyraFlux::Validate(Broken);
			ASSERT_THAT(AreEqual(2, Problems.Num()));
			ASSERT_THAT(IsTrue(Problems[0].StartsWith(TEXT("/grants/laneSpire/durationSeconds")) && Problems[1].StartsWith(TEXT("/grants/inhibitor/durationSeconds"))));
		}

		TEST_METHOD(SpellSlotsOpenAsPermanentFluxReachesTheirRisingThresholds)
		{
			// Fixture thresholds, as canon's (Battleground Bible §14).
			const TArray<double> Thresholds = { 25.0, 75.0 };
			ASSERT_THAT(AreEqual(0, VeyraFlux::UnlockedSpellSlots(0.0, Thresholds)));
			ASSERT_THAT(AreEqual(0, VeyraFlux::UnlockedSpellSlots(24.0, Thresholds)));
			ASSERT_THAT(AreEqual(1, VeyraFlux::UnlockedSpellSlots(25.0, Thresholds), TEXT("a threshold reached opens its slot")));
			ASSERT_THAT(AreEqual(1, VeyraFlux::UnlockedSpellSlots(74.0, Thresholds)));
			ASSERT_THAT(AreEqual(2, VeyraFlux::UnlockedSpellSlots(500.0, Thresholds)));

			FVeyraFluxTuning Broken = UVeyraFluxTuningSubsystem::Get();
			Broken.SpellSlots.Thresholds = { 75.0, 25.0 };
			ASSERT_THAT(IsTrue(VeyraFlux::Validate(Broken).ContainsByPredicate([](const FString& Problem) { return Problem.StartsWith(TEXT("/spellSlots/thresholds/1")); })));
			Broken.SpellSlots.Thresholds = { 25.0 };
			ASSERT_THAT(IsTrue(VeyraFlux::Validate(Broken).ContainsByPredicate([](const FString& Problem) { return Problem.StartsWith(TEXT("/spellSlots/thresholds:")); })));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
