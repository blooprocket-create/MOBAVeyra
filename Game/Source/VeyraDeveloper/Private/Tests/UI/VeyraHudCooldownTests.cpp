// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Hud/VeyraHudModel.h"

namespace VeyraHudCooldownTests
{
	// Veyra.UI.HudCooldowns.*: the countdown's precision and the sweep over a cooling tile (Settings Bible §3.4, Proposal 44;
	// ADR-059 §3).
	TEST_CLASS(HudCooldowns, "Veyra.UI")
	{
		TEST_METHOD(TheCountdownShowsTenthsOnlyBelowTenSecondsAndOnlyWhenAsked)
		{
			ASSERT_THAT(AreEqual(VeyraHud::CooldownLabel(12.3, true), FString(TEXT("13")), TEXT("whole seconds from ten on, rounded up")));
			ASSERT_THAT(AreEqual(VeyraHud::CooldownLabel(10.0, true), FString(TEXT("10"))));
			ASSERT_THAT(AreEqual(VeyraHud::CooldownLabel(4.24, true), FString(TEXT("4.2"))));
			ASSERT_THAT(AreEqual(VeyraHud::CooldownLabel(4.24, false), FString(TEXT("5")), TEXT("Whole rounds up, so it never shows 0 while cooling")));
			ASSERT_THAT(AreEqual(VeyraHud::CooldownLabel(0.3, false), FString(TEXT("1"))));
		}

		TEST_METHOD(TheSweepCoversWhatIsStillToWait)
		{
			const FVector2D TopLeft(100.0, 200.0);
			constexpr float Side = 40.0f;
			const FVector2D Centre = TopLeft + FVector2D(Side / 2.0f);
			// Just started: the whole tile, from twelve o'clock round through every corner back to twelve.
			const TArray<FVector2D> Whole = VeyraHud::SweepOutline(TopLeft, Side, 0.0);
			ASSERT_THAT(AreEqual(7, Whole.Num()));
			ASSERT_THAT(IsTrue(Whole[0].Equals(Centre) && Whole[1].Equals(FVector2D(120.0, 200.0), 0.001) && Whole.Last().Equals(FVector2D(120.0, 200.0), 0.001)));
			ASSERT_THAT(IsTrue(Whole[2].Equals(FVector2D(140.0, 200.0), 0.001) && Whole[4].Equals(FVector2D(100.0, 240.0), 0.001), TEXT("top right, then bottom left")));
			// Half done: the left half, from six o'clock round to twelve.
			const TArray<FVector2D> Half = VeyraHud::SweepOutline(TopLeft, Side, 0.5);
			ASSERT_THAT(AreEqual(5, Half.Num()));
			ASSERT_THAT(IsTrue(Half[1].Equals(FVector2D(120.0, 240.0), 0.001)));
			for (const FVector2D& Point : Half)
			{
				ASSERT_THAT(IsTrue(Point.X <= Centre.X + 0.001, Point.ToString()));
			}
			// Three quarters done: the top-left quarter; then nothing once it is ready.
			const TArray<FVector2D> Quarter = VeyraHud::SweepOutline(TopLeft, Side, 0.75);
			ASSERT_THAT(IsTrue(Quarter.Num() == 4 && Quarter[1].Equals(FVector2D(100.0, 220.0), 0.001) && Quarter[2].Equals(TopLeft, 0.001)));
			ASSERT_THAT(IsTrue(VeyraHud::SweepOutline(TopLeft, Side, 1.0).IsEmpty() && VeyraHud::SweepOutline(TopLeft, Side, 1.5).IsEmpty()));
		}
	};
}

#endif