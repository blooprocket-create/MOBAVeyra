// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

// VeyraUI is client only: a server build has neither it nor these tests.
#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Components/ActorTestSpawner.h"
#include "Hud/VeyraMinimapModel.h"
#include "Layout/VeyraLayout.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"

namespace VeyraMinimapTests
{
	// Veyra.UI.Minimap.*: the minimap's geometry and what it draws (Settings Bible §3.2; ADR-020 §2).
	TEST_CLASS(Minimap, "Veyra.UI")
	{
		TEST_METHOD(TheMapIsOrientedAsTheCameraLooksAndRoundTrips)
		{
			const FVeyraMinimapFrame Frame = VeyraMinimap::FrameFor(FVector2D(1920.0, 1080.0), 200.0, 20.0, 5000.0);
			ASSERT_THAT(IsTrue(Frame.Origin.Equals(FVector2D(1700.0, 860.0)), TEXT("bottom-right, inside the margin")));
			ASSERT_THAT(IsTrue(VeyraMinimap::ToMap(Frame, FVector::ZeroVector).Equals(FVector2D(1800.0, 960.0)), TEXT("the centre in the middle")));
			ASSERT_THAT(IsTrue(VeyraMinimap::ToMap(Frame, FVector(5000.0, 0.0, 0.0)).Equals(FVector2D(1800.0, 860.0)), TEXT("+X is up")));
			ASSERT_THAT(IsTrue(VeyraMinimap::ToMap(Frame, FVector(0.0, 5000.0, 0.0)).Equals(FVector2D(1900.0, 960.0)), TEXT("+Y is right")));
			const FVector Point(1234.0, -2345.0, 0.0);
			const TOptional<FVector> Back = VeyraMinimap::ToWorld(Frame, VeyraMinimap::ToMap(Frame, Point));
			ASSERT_THAT(IsTrue(Back.IsSet() && Back->Equals(Point, 0.01)));
			ASSERT_THAT(IsFalse(VeyraMinimap::ToWorld(Frame, FVector2D(100.0, 100.0)).IsSet(), TEXT("a click off the map is no point on it")));
		}

		TEST_METHOD(EachDotIsTheViewersOwnAnAllysAnEnemysOrNeutral)
		{
			ASSERT_THAT(IsTrue(VeyraMinimap::SideOf(EVeyraTeam::A, EVeyraTeam::A, true) == EVeyraMinimapSide::Own));
			ASSERT_THAT(IsTrue(VeyraMinimap::SideOf(EVeyraTeam::A, EVeyraTeam::A, false) == EVeyraMinimapSide::Ally));
			ASSERT_THAT(IsTrue(VeyraMinimap::SideOf(EVeyraTeam::A, EVeyraTeam::B, false) == EVeyraMinimapSide::Enemy));
			ASSERT_THAT(IsTrue(VeyraMinimap::SideOf(EVeyraTeam::A, EVeyraTeam::None, false) == EVeyraMinimapSide::Neutral));
		}

		TEST_METHOD(ATeammatesPingFadesOverThePlayersPersistenceThenGoes)
		{
			const FVeyraMinimapFrame Frame = VeyraMinimap::FrameFor(FVector2D(1920.0, 1080.0), 200.0, 20.0, 5000.0);
			FVeyraReceivedPing Fresh;
			Fresh.Ping.Point = FVector(5000.0, 0.0, 0.0);
			Fresh.Ping.Kind = EVeyraPingKind::Danger;
			Fresh.ReceivedAt = 9.0;
			FVeyraReceivedPing Old;
			Old.ReceivedAt = 5.0;
			const double Persistence = 4.0;
			const TArray<FVeyraMinimapTeamPing> Drawn = VeyraMinimap::DescribeTeamPings(Frame, { Fresh, Old }, 10.0, Persistence);
			ASSERT_THAT(AreEqual(Drawn.Num(), 1, TEXT("one past the player's persistence is gone")));
			ASSERT_THAT(IsTrue(Drawn[0].Centre.Equals(FVector2D(1800.0, 860.0)) && Drawn[0].Kind == EVeyraPingKind::Danger));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Drawn[0].Fade, 0.75)));
		}

		TEST_METHOD(ItDrawsTheLanesAndWhereTheCameraLooks)
		{
			FActorTestSpawner Spawner;
			const FVeyraMinimapFrame Frame = VeyraMinimap::FrameFor(FVector2D(1920.0, 1080.0), 200.0, 20.0, 5000.0);
			const FVeyraMinimapView View = VeyraMinimap::Describe(Spawner.GetWorld(), Frame, EVeyraTeam::A, nullptr, FVector(0.0, 0.0, 0.0), 0.0);
			ASSERT_THAT(IsTrue(View.Lanes.Num() == 3 && View.Lanes[0].Points.Num() >= 2, TEXT("the battleground's three lanes")));
			ASSERT_THAT(IsTrue(View.Focus.IsSet() && View.Focus->Equals(FVector2D(1800.0, 960.0))));
			ASSERT_THAT(IsTrue(View.Dots.IsEmpty(), TEXT("a world with no units has no dots")));
			// The river crosses from corner to corner, and every wall of both halves stands on it (ADR-043 §4).
			const FVeyraBattlegroundLayout& Layout = UVeyraWorldTuningSubsystem::Get().Layout;
			ASSERT_THAT(IsTrue(View.River.Num() == 6 && View.River.Contains(FVector2D(1700.0, 860.0)) && View.River.Contains(FVector2D(1900.0, 1060.0)),
				TEXT("from the top-left corner to the bottom-right")));
			for (const FVector2D& Corner : View.River)
			{
				ASSERT_THAT(IsTrue(Corner.X >= 1700.0 - 1e-6 && Corner.X <= 1900.0 + 1e-6 && Corner.Y >= 860.0 - 1e-6 && Corner.Y <= 1060.0 + 1e-6, TEXT("cut to the map")));
			}
			ASSERT_THAT(AreEqual(View.Walls.Num(), Layout.Walls.Num() * 2));
			const FVeyraTerrainBox First = VeyraLayout::Walls(Layout)[0];
			ASSERT_THAT(IsTrue(View.Walls[0].Corners.Num() == 4
				&& View.Walls[0].Corners[0].Equals(VeyraMinimap::ToMap(Frame, FVector(First.Corners()[0], 0.0)))));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI
