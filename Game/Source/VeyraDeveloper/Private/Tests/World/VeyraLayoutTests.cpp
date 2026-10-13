// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Algo/NoneOf.h"
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

		// How far a point on an edge may read as off it, from rounding in its distance. Fixture value.
		static constexpr double EdgeSlack = 0.01;

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
			ASSERT_THAT(IsTrue(VeyraLayout::DirectionAlong(Lane.Points, 40.0).Equals(FVector2D(1.0, 0.0), Tolerance)));
			ASSERT_THAT(IsTrue(VeyraLayout::DirectionAlong(Lane.Points, 100.0).Equals(FVector2D(1.0, 0.0), Tolerance), TEXT("at a bend, the way it arrived")));
			ASSERT_THAT(IsTrue(VeyraLayout::DirectionAlong(Lane.Points, 120.0).Equals(FVector2D(0.0, 1.0), Tolerance)));
			ASSERT_THAT(IsTrue(VeyraLayout::DirectionAlong(Lane.Points, 1000.0).Equals(FVector2D(0.0, 1.0), Tolerance), TEXT("past the end, its last way")));
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

		TEST_METHOD(MassesAreTheirOwnHalfTurnAndTheirEdgeBoxesRingThemFromInside)
		{
			// The masses (ADR-073 §2, §3): each one's half turn is a mass too, and its edge boxes lie inside it, no longer
			// than the rule allows, every point of its outline in one, so whatever crosses its edge meets a box.
			const FVeyraBattlegroundLayout& Layout = Committed();
			const FVeyraMassRules& Rules = Layout.MassRules;
			const TArray<FVeyraMassShape> Masses = VeyraLayout::Masses(Layout);
			ASSERT_THAT(IsFalse(Masses.IsEmpty()));
			for (const FVeyraMassShape& Mass : Masses)
			{
				ASSERT_THAT(IsTrue(Masses.ContainsByPredicate([&Mass](const FVeyraMassShape& Other) {
					return Other.Kind == Mass.Kind && Other.Outline.Num() == Mass.Outline.Num() && Other.Outline[0].Equals(VeyraLayout::Rotate(Mass.Outline[0]), Tolerance);
				})));
				const TArray<FVeyraTerrainBox> Boxes = Mass.EdgeBoxes(Rules.EdgeThickness, Rules.EdgeMaxLength);
				for (const FVeyraTerrainBox& Box : Boxes)
				{
					ASSERT_THAT(IsTrue(Box.Length <= Rules.EdgeMaxLength + Tolerance && Box.Thickness > 0.0 && Box.Thickness <= Rules.EdgeThickness + Tolerance));
					for (const FVector2D& Corner : Box.Corners())
					{
						ASSERT_THAT(IsTrue(Mass.DepthInside(Corner) >= -EdgeSlack, *FString::Printf(TEXT("a box's corner at %s stands out of its mass"), *Corner.ToString())));
					}
				}
				for (int32 Corner = 0; Corner < Mass.Outline.Num(); ++Corner)
				{
					const FVector2D From = Mass.Outline[Corner];
					const FVector2D To = Mass.Outline[(Corner + 1) % Mass.Outline.Num()];
					for (double Share = 0.0; Share <= 1.0; Share += 0.125)
					{
						const FVector2D Point = FMath::Lerp(From, To, Share);
						ASSERT_THAT(IsTrue(Boxes.ContainsByPredicate([&Point](const FVeyraTerrainBox& Box) { return Box.DistanceTo(Point) <= EdgeSlack; }),
							*FString::Printf(TEXT("the edge at %s is in no box"), *Point.ToString())));
					}
				}
			}
		}

		TEST_METHOD(AnEdgeSplitsItsLongSpansAndFacesIn)
		{
			// A square mass, counter-clockwise: each side in pieces no longer than the most, each as deep as asked, just inside.
			FVeyraMassShape Square;
			Square.Outline = { { 0.0, 0.0 }, { 1000.0, 0.0 }, { 1000.0, 1000.0 }, { 0.0, 1000.0 } };
			const TArray<FVeyraTerrainBox> Boxes = Square.EdgeBoxes(100.0, 300.0);
			ASSERT_THAT(AreEqual(16, Boxes.Num(), TEXT("four sides of four pieces")));
			const FVeyraTerrainBox& First = Boxes[0];
			ASSERT_THAT(IsTrue(First.Facing.Equals(FVector2D(0.0, 1.0), Tolerance) && FMath::IsNearlyEqual(First.Centre.Y, 50.0, Tolerance)
				&& FMath::IsNearlyEqual(First.Length, 250.0, Tolerance), TEXT("the first piece of the first side lies along it, facing in")));
			ASSERT_THAT(IsNear(Square.DepthInside(FVector2D(500.0, 500.0)), 500.0, Tolerance));
			ASSERT_THAT(IsNear(Square.DepthInside(FVector2D(-10.0, 500.0)), -10.0, Tolerance, TEXT("negative outside")));
			ASSERT_THAT(AreEqual(6, Square.Triangles().Num(), TEXT("a square fills as two triangles")));
		}

		TEST_METHOD(TheTerrainIsEveryMassEdgeTheKeepWallsAndTheRim)
		{
			const FVeyraBattlegroundLayout& Layout = Committed();
			int32 EdgeBoxes = 0;
			for (const FVeyraMassShape& Mass : VeyraLayout::Masses(Layout))
			{
				EdgeBoxes += Mass.EdgeBoxes(Layout.MassRules.EdgeThickness, Layout.MassRules.EdgeMaxLength).Num();
			}
			const TArray<FVeyraTerrainBox> Rim = VeyraLayout::Rim(Layout);
			ASSERT_THAT(AreEqual(VeyraLayout::Walls(Layout).Num(), EdgeBoxes + VeyraLayout::KeepWalls(Layout).Num() + Rim.Num()));
			// The rim stands just outside the playable square, closing the band behind the keep, whose back meets the square.
			const double Playable = VeyraLayout::PlayableHalfExtent(Layout);
			for (const FVeyraTerrainBox& Box : Rim)
			{
				for (const FVector2D& Corner : Box.Corners())
				{
					ASSERT_THAT(IsTrue(FMath::Max(FMath::Abs(Corner.X), FMath::Abs(Corner.Y)) >= Playable - EdgeSlack));
				}
			}
			const FVector2D BehindTheKeep(-Playable - Layout.MassRules.EdgeThickness / 2.0, VeyraLayout::ToVector(Layout.Base.PrimeWell).Y);
			ASSERT_THAT(IsTrue(Rim.ContainsByPredicate([&BehindTheKeep](const FVeyraTerrainBox& Box) { return Box.DistanceTo(BehindTheKeep) == 0.0; })));
		}

		TEST_METHOD(TheKeepsWallsLeaveItsGatesOpenAndItsBaseInside)
		{
			const FVeyraBattlegroundLayout& Layout = Committed();
			const TArray<FVeyraTerrainBox> Walls = VeyraLayout::KeepWalls(Layout);
			ASSERT_THAT(AreEqual(Layout.Lanes.Num(), Layout.Keep.Gates.Num()));
			for (const FVeyraKeepGateLayout& Gate : Layout.Keep.Gates)
			{
				// A road's width open through each gate, for both teams.
				const FVeyraLaneLayout* Lane = Layout.Lanes.FindByPredicate([&Gate](const FVeyraLaneLayout& Each) { return Each.Lane == Gate.Lane; });
				ASSERT_THAT(IsTrue(Lane != nullptr && Gate.Width >= Lane->Width));
				for (const EVeyraTeam Team : { EVeyraTeam::A, EVeyraTeam::B })
				{
					const FVector2D Centre = VeyraLayout::ForTeam(VeyraLayout::ToVector(Gate.Center), Team);
					ASSERT_THAT(IsTrue(Algo::NoneOf(Walls, [&](const FVeyraTerrainBox& Box) { return Box.DistanceTo(Centre) < Lane->Width / 2.0 - EdgeSlack; })));
				}
			}
			for (const EVeyraTeam Team : { EVeyraTeam::A, EVeyraTeam::B })
			{
				ASSERT_THAT(IsTrue(VeyraLayout::IsInKeep(Layout, VeyraLayout::ForTeam(VeyraLayout::ToVector(Layout.Base.PrimeWell), Team))));
				ASSERT_THAT(IsFalse(VeyraLayout::IsJungle(Layout, VeyraLayout::Fountain(Layout, Team)), TEXT("a keep is no jungle")));
			}
		}

		TEST_METHOD(ValidationRefusesBrokenSpacesAndMasses)
		{
			const auto Problems = [](const FVeyraWorldTuning& Tuning) { return FString::Join(VeyraWorld::Validate(Tuning), TEXT(" | ")); };

			// A mass with a needle's corner, standing in a clearing, that has no half turn.
			FVeyraWorldTuning Needle = UVeyraWorldTuningSubsystem::Get();
			const FVeyraClearingLayout& Clearing = Needle.Layout.Clearings[0];
			const FVector2D Ground = VeyraLayout::ToVector(Clearing.Center);
			FVeyraMassLayout& Added = Needle.Layout.Masses.Items.AddDefaulted_GetRef();
			Added.Id = TEXT("z00");
			Added.Points = { { Ground.X, Ground.Y }, { Ground.X + 600.0, Ground.Y }, { Ground.X + 600.0, Ground.Y + 100.0 } };
			const FString Pointer = FString::Printf(TEXT("/layout/masses/items/%d"), Needle.Layout.Masses.Items.Num() - 1);
			const FString NeedleProblems = Problems(Needle);
			ASSERT_THAT(IsTrue(NeedleProblems.Contains(Pointer + TEXT("/points: no corner of a mass may be sharper than a right angle")), NeedleProblems));
			ASSERT_THAT(IsTrue(NeedleProblems.Contains(Pointer + TEXT(": its half turn must be a mass of the same kind")), NeedleProblems));
			ASSERT_THAT(IsTrue(NeedleProblems.Contains(Pointer + TEXT(": its outline runs")), NeedleProblems));

			// A clearing's id twice; a camp off its clearing; a gate missing; the rules and the generated set out of shape.
			FVeyraWorldTuning Broken = UVeyraWorldTuningSubsystem::Get();
			Broken.Layout.Clearings[1].Id = Broken.Layout.Clearings[0].Id;
			Broken.Wildlife.Camps.Last().Center.X += 300.0;
			Broken.Layout.Keep.Gates.RemoveAt(0);
			Broken.Layout.MassRules.EdgeThickness = Broken.Layout.MassRules.MinThickness;
			Broken.Layout.Masses.InputSha256.Reset();
			Broken.Layout.WallHalfHeight = 0.0;
			Broken.Layout.River.WalkableEnd = Broken.Layout.HalfExtent * 4.0;
			const FString All = Problems(Broken);
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/layout/clearings/1/id: each clearing needs an id of its own")), All));
			ASSERT_THAT(IsTrue(All.Contains(FString::Printf(TEXT("/wildlife/camps/%d/center: a camp stands at the centre of the clearing of its id"), Broken.Wildlife.Camps.Num() - 1)), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("lane needs exactly one gate, not 0")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/layout/massRules/edgeThickness: at most half minThickness")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/layout/masses/inputSha256: must be the hash")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/layout/wallHalfHeight: the terrain needs a height")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/layout/river/walkableEnd: must be above 0 and end before the main channel does")), All));
		}

		TEST_METHOD(SpiresStandOnTheShoulderAndTheHalfTurnCarriesEachOntoItsCounterpart)
		{
			const FVeyraBattlegroundLayout& Layout = Committed();
			for (const FVeyraStructurePlacement& Spire : VeyraLayout::Structures(Layout))
			{
				if (Spire.Kind != EVeyraStructureKind::LaneSpire)
				{
					continue;
				}
				const FVeyraLaneLayout* Lane = Layout.Lanes.FindByPredicate([&Spire](const FVeyraLaneLayout& Each) { return Spire.Lane == Each.Lane; });
				const double FromRoad = VeyraLayout::DistanceToPath(Lane->Points, Spire.Location);
				ASSERT_THAT(IsTrue(FromRoad > Lane->Width / 2.0 && FromRoad <= Lane->Width / 2.0 + Layout.Terrain.LaneShoulder + Tolerance, TEXT("beside the road, on its shoulder")));
			}
			// Top and bottom list different sides: Team B's bottom Spires no longer stand where Team A's top ones do.
			FVeyraWorldTuning Lopsided = UVeyraWorldTuningSubsystem::Get();
			FVeyraLaneLayout* Top = Lopsided.Layout.Lanes.FindByPredicate([](const FVeyraLaneLayout& Each) { return Each.Lane == EVeyraLane::Top; });
			Top->SpireSides[0] = -Top->SpireSides[0];
			Lopsided.Layout.SpireOffset = 0.0;
			const FString All = FString::Join(VeyraWorld::Validate(Lopsided), TEXT(" | "));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("spireOffset must stand each Spire on the lane's shoulder")), All));
			Lopsided.Layout.SpireOffset = Committed().SpireOffset;
			const FString Turned = FString::Join(VeyraWorld::Validate(Lopsided), TEXT(" | "));
			ASSERT_THAT(IsTrue(Turned.Contains(TEXT("turned half a turn must be Team B's")), Turned));
		}

		TEST_METHOD(ASpireJustShortOfABendStandsSquareToTheRoadItIsOn)
		{
			// Its side is taken from the road's own segment, so no sampling distance can tilt it toward the bend ahead.
			static constexpr double ShortOfTheBend = 2.0;
			static constexpr double Leg = 1000.0;
			FVeyraBattlegroundLayout Layout = Committed();
			FVeyraLaneLayout Bent = Layout.Lanes[0];
			Bent.Points = { { 0.0, 0.0 }, { Leg, 0.0 }, { Leg, Leg } };
			Bent.InhibitorDistance = 0.0;
			Bent.SpireDistances = { Leg - ShortOfTheBend };
			Bent.SpireSides = { 1 };
			Layout.Lanes = { Bent };
			const FVeyraStructurePlacement* Spire = VeyraLayout::Structures(Layout).FindByPredicate([](const FVeyraStructurePlacement& P) {
				return P.Team == EVeyraTeam::A && P.Kind == EVeyraStructureKind::LaneSpire;
			});
			const FVector2D Expected(Leg - ShortOfTheBend, Layout.SpireOffset);
			ASSERT_THAT(IsTrue(Spire && Spire->Location.Equals(Expected, Tolerance),
				Spire ? *FString::Printf(TEXT("stands at %s, not %s"), *Spire->Location.ToString(), *Expected.ToString()) : TEXT("no Spire")));
		}

		TEST_METHOD(MapFogCannotCoverALaneOrItsShoulder)
		{
			FVeyraWorldTuning Broken = UVeyraWorldTuningSubsystem::Get();
			const FVeyraLaneLayout& Lane = Broken.Layout.Lanes[0];
			FVeyraFogLayout& Fog = Broken.Layout.DenseFog[0];
			const FVector2D Start = VeyraLayout::ToVector(Lane.Points[0]);
			const FVector2D End = VeyraLayout::ToVector(Lane.Points[1]);
			const FVector2D Mid = (Start + End) / 2.0;
			const FVector2D Across = FVector2D(-(End - Start).GetSafeNormal().Y, (End - Start).GetSafeNormal().X);
			const FString Expected = FString::Printf(TEXT("/layout/denseFog/0: the fog must stay off the %s lane's road and shoulders"), *UEnum::GetValueAsString(Lane.Lane));
			Fog.Center = { Mid.X, Mid.Y };
			ASSERT_THAT(IsTrue(VeyraWorld::Validate(Broken).Contains(Expected)));
			// Its edge over the shoulder only, its centre beyond the road.
			const FVector2D Shoulder = Mid + Across * (Lane.Width / 2.0 + Fog.Radius + Broken.Layout.Terrain.LaneShoulder / 2.0);
			Fog.Center = { Shoulder.X, Shoulder.Y };
			ASSERT_THAT(IsTrue(VeyraWorld::Validate(Broken).Contains(Expected)));
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
