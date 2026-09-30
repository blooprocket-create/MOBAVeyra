// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Layout/VeyraLayout.h"
#include "Tests/World/VeyraBattlegroundTestLayout.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraWorldTests
{
	// Veyra.World.JungleTerrain.*: jungle terrain, derived from the layout (ADR-026 §5): the floor that
	// is no lane, river or base.
	TEST_CLASS(JungleTerrain, "Veyra.World")
	{
		TEST_METHOD(JungleIsTheGroundBetweenTheLanesTheRiverAndTheBases)
		{
			// The compact battleground's mid lane runs along Y = X and its river along Y = -X; fixture points.
			const FVeyraBattlegroundLayout Layout = CompactBattleground();
			ASSERT_THAT(IsTrue(VeyraLayout::IsJungle(Layout, FVector2D(1000.0, -200.0)), TEXT("between the lane and the river, away from both bases")));
			ASSERT_THAT(IsTrue(VeyraLayout::IsJungle(Layout, FVector2D(-200.0, 1000.0)), TEXT("and its mirror")));
			ASSERT_THAT(IsFalse(VeyraLayout::IsJungle(Layout, FVector2D(500.0, 500.0)), TEXT("the lane")));
			ASSERT_THAT(IsFalse(VeyraLayout::IsJungle(Layout, FVector2D(700.0, -700.0)), TEXT("the river")));
			ASSERT_THAT(IsFalse(VeyraLayout::IsJungle(Layout, FVector2D(-2400.0, -1900.0)), TEXT("Team A's base")));
			ASSERT_THAT(IsFalse(VeyraLayout::IsJungle(Layout, FVector2D(2400.0, 1900.0)), TEXT("Team B's base")));
			ASSERT_THAT(IsFalse(VeyraLayout::IsJungle(Layout, FVector2D(3500.0, 0.0)), TEXT("off the floor")));
		}

		TEST_METHOD(TheCommittedCampsStandInTheJungleAndTheLanesDoNot)
		{
			const FVeyraWorldTuning& Tuning = UVeyraWorldTuningSubsystem::Get();
			const FVeyraBattlegroundLayout& Layout = Tuning.Layout;
			for (const EVeyraTeam Team : { EVeyraTeam::A, EVeyraTeam::B })
			{
				for (const FVeyraCampTuning& Camp : Tuning.Wildlife.Camps)
				{
					const FVector2D Center = VeyraLayout::ForTeam(VeyraLayout::ToVector(Camp.Center), Team);
					ASSERT_THAT(IsTrue(VeyraLayout::IsJungle(Layout, Center), *Center.ToString()));
				}
				ASSERT_THAT(IsFalse(VeyraLayout::IsJungle(Layout, VeyraLayout::Fountain(Layout, Team))));
			}
			for (const FVeyraLaneLayout& Lane : Layout.Lanes)
			{
				ASSERT_THAT(IsFalse(VeyraLayout::IsJungle(Layout, VeyraLayout::PointAlong(Lane.Points, VeyraLayout::Length(Lane.Points) / 3.0))));
			}
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
