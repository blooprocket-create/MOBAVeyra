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
	};
}

#endif // WITH_AUTOMATION_WORKER
