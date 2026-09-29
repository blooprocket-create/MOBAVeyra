// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "Hud/VeyraMinimapModel.h"

#if WITH_AUTOMATION_WORKER

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

		TEST_METHOD(ItDrawsTheLanesAndWhereTheCameraLooks)
		{
			FActorTestSpawner Spawner;
			const FVeyraMinimapFrame Frame = VeyraMinimap::FrameFor(FVector2D(1920.0, 1080.0), 200.0, 20.0, 5000.0);
			const FVeyraMinimapView View = VeyraMinimap::Describe(Spawner.GetWorld(), Frame, EVeyraTeam::A, nullptr, FVector(0.0, 0.0, 0.0), 0.0);
			ASSERT_THAT(IsTrue(View.Lanes.Num() == 3 && View.Lanes[0].Points.Num() >= 2, TEXT("the battleground's three lanes")));
			ASSERT_THAT(IsTrue(View.Focus.IsSet() && View.Focus->Equals(FVector2D(1800.0, 960.0))));
			ASSERT_THAT(IsTrue(View.Dots.IsEmpty(), TEXT("a world with no units has no dots")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
