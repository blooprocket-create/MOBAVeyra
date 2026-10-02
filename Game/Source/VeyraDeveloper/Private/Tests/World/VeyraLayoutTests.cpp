// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Engine/Engine.h"
#include "Layout/VeyraLayout.h"
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

		static const FVeyraBattlegroundLayout& Committed()
		{
			return UVeyraWorldTuningSubsystem::Get().Layout;
		}

		TEST_METHOD(TheCommittedFileLoads)
		{
			ASSERT_THAT(IsTrue(GEngine->GetEngineSubsystem<UVeyraWorldTuningSubsystem>()->IsLoaded()));
			ASSERT_THAT(IsTrue(VeyraWorld::Validate(UVeyraWorldTuningSubsystem::Get()).IsEmpty()));
		}

		TEST_METHOD(TheMirrorSwapsTheBasesAndKeepsTheRiver)
		{
			const FVector2D Corner(-1000.0, -1000.0);
			ASSERT_THAT(IsTrue(VeyraLayout::Mirror(Corner).Equals(FVector2D(1000.0, 1000.0), Tolerance)));
			const FVector2D OnRiver(700.0, -700.0);
			ASSERT_THAT(IsTrue(VeyraLayout::Mirror(OnRiver).Equals(OnRiver, Tolerance), TEXT("the river's diagonal is its own mirror")));
			ASSERT_THAT(IsTrue(VeyraLayout::Mirror(VeyraLayout::Mirror(Corner)).Equals(Corner, Tolerance)));
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

		TEST_METHOD(EachTeamHasEveryStructureAndTeamBsMirrorTeamAs)
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
				ASSERT_THAT(IsTrue(VeyraLayout::Mirror(TeamA[Index].Location).Equals(TeamB[Index].Location, Tolerance)));
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
			ASSERT_THAT(IsTrue(Mentions(TEXT("/layout/lanes/1/points: the lane must be its own mirror")), All));
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
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraLayout::DepthInTeamAHalf(Layout, FVector2D(300.0, -300.0)), 0.0, Tolerance), TEXT("on the river")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraLayout::DepthInTeamAHalf(Layout, FVector2D(100.0, 100.0)), -200.0 / UE_SQRT_2, Tolerance)));
		}

		TEST_METHOD(CampsStayOnTeamAsHalfOffTheLanesAndWellsOnTheRiver)
		{
			FVeyraWorldTuning Broken = UVeyraWorldTuningSubsystem::Get();
			ASSERT_THAT(IsTrue(Broken.Wildlife.Camps.Num() >= 3 && !Broken.FluxWells.Sites.IsEmpty()));
			// On Team B's half; on the mid lane; of no species.
			Broken.Wildlife.Camps[0].Center = { 3000.0, 3000.0 };
			Broken.Wildlife.Camps[1].Center = { -2000.0, -2000.0 };
			Broken.Wildlife.Camps[2].Species = FVeyraContentId::FromText(TEXT("unicorn")).GetValue();
			Broken.FluxWells.Sites[0] = { 3000.0, -1000.0 };
			const TArray<FString> Problems = VeyraWorld::Validate(Broken);
			const FString All = FString::Join(Problems, TEXT(" | "));
			const auto Mentions = [&Problems](const TCHAR* Text) { return Problems.ContainsByPredicate([Text](const FString& Problem) { return Problem.Contains(Text); }); };
			ASSERT_THAT(IsTrue(Mentions(TEXT("/wildlife/camps/0/center: the camp must lie on Team A's half")), All));
			ASSERT_THAT(IsTrue(Mentions(TEXT("/wildlife/camps/1/center: the camp's leash must stay clear of the EVeyraLane::Mid lane")), All));
			ASSERT_THAT(IsTrue(Mentions(TEXT("/wildlife/camps/2/species: unicorn is no species")), All));
			ASSERT_THAT(IsTrue(Mentions(TEXT("/fluxWells/sites/0: must lie on the river")), All));
		}

		TEST_METHOD(WallsLieInTeamAsHalfAndTeamBsMirrorThem)
		{
			// The battleground's walls (ADR-043 §1): Team A's, then their mirrors.
			const FVeyraBattlegroundLayout& Layout = Committed();
			ASSERT_THAT(IsFalse(Layout.Walls.IsEmpty()));
			const TArray<FVeyraTerrainBox> Walls = VeyraLayout::Walls(Layout);
			ASSERT_THAT(AreEqual(Walls.Num(), Layout.Walls.Num() * 2));
			for (int32 Index = 0; Index < Layout.Walls.Num(); ++Index)
			{
				const FVeyraTerrainBox& A = Walls[Index];
				const FVeyraTerrainBox& B = Walls[Index + Layout.Walls.Num()];
				for (const FVector2D& Corner : A.Corners())
				{
					ASSERT_THAT(IsTrue(VeyraLayout::DepthInTeamAHalf(Layout, Corner) > 0.0));
				}
				ASSERT_THAT(IsTrue(B.Centre.Equals(VeyraLayout::Mirror(A.Centre), Tolerance) && B.Facing.Equals(VeyraLayout::Mirror(A.Facing), Tolerance)));
				ASSERT_THAT(IsTrue(B.Length == A.Length && B.Thickness == A.Thickness));
				// Each corner of Team B's wall is the mirror of one of Team A's.
				for (const FVector2D& Corner : B.Corners())
				{
					ASSERT_THAT(IsTrue(A.DistanceTo(VeyraLayout::Mirror(Corner)) < 1e-3));
				}
			}
		}

		TEST_METHOD(AWallKeepsClearOfWhatStandsOnTheBattleground)
		{
			FVeyraWorldTuning Broken = UVeyraWorldTuningSubsystem::Get();
			const FVeyraMapPoint Camp = Broken.Wildlife.Camps[0].Center;
			const double Edge = Broken.Layout.HalfExtent;
			Broken.Layout.Walls = {
				{ { 0.0, 0.0 }, 0.0, 300.0, 100.0 },         // across the dividing line
				{ { -2000.0, -2000.0 }, 45.0, 300.0, 100.0 }, // on the mid lane
				{ Camp, 0.0, 300.0, 100.0 },                  // on a camp
				{ { -Edge, -1000.0 }, 0.0, 300.0, 100.0 },    // off the floor's edge
				{ { -3000.0, -6000.0 }, 0.0, 0.0, 100.0 },    // no length
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
			ASSERT_THAT(IsTrue(Mentions(TEXT("/layout/walls/4: a wall needs a length and a thickness")), All));
		}

		TEST_METHOD(DenseFogLiesInTeamAsHalfAndTeamBsMirrorsIt)
		{
			// The battleground's bush (Battleground Bible §11): Team A's circles, then their mirrors.
			const FVeyraBattlegroundLayout& Layout = UVeyraWorldTuningSubsystem::Get().Layout;
			ASSERT_THAT(IsFalse(Layout.DenseFog.IsEmpty()));
			const TArray<FVeyraFogPlacement> Fog = VeyraLayout::DenseFog(Layout);
			ASSERT_THAT(AreEqual(Fog.Num(), Layout.DenseFog.Num() * 2));
			for (int32 Index = 0; Index < Layout.DenseFog.Num(); ++Index)
			{
				const FVeyraFogPlacement& A = Fog[Index];
				const FVeyraFogPlacement& B = Fog[Index + Layout.DenseFog.Num()];
				ASSERT_THAT(IsTrue(VeyraLayout::DepthInTeamAHalf(Layout, A.Center) > A.Radius));
				ASSERT_THAT(IsTrue(B.Center.Equals(VeyraLayout::Mirror(A.Center)) && B.Radius == A.Radius));
			}

			// Across the river, off the floor, or touching the dividing line (and so its mirror) is refused.
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
