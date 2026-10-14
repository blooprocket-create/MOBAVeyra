// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Greybox/VeyraGreyboxSettings.h"
#include "Hud/VeyraHudMotion.h"
#include "Hud/VeyraStatGlyphs.h"

namespace VeyraHudMotionTests
{
	// Veyra.UI.HudMotion.*: how the HUD moves between frames (ADR-082 §1-§2): its timing curves, the moments it remembers, a
	// bar's trail and a count that runs.
	TEST_CLASS(HudMotion, "Veyra.UI")
	{
		// Fixture values: a frame's start, a frame's length, and seconds to hold and drain over.
		static constexpr double Start = 100.0;
		static constexpr double Frame = 1.0 / 60.0;
		static constexpr double Hold = 0.4;
		static constexpr double Drain = 0.3;

		static FVeyraEaseCurve Linear()
		{
			return FVeyraEaseCurve{ 0.0f, 0.0f, 1.0f, 1.0f };
		}

		TEST_METHOD(ACurveRunsFromNoneToAllAndKeepsItsShape)
		{
			// The snap of the committed settings: quick out of the start, so a quarter in it is well past a quarter.
			const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
			ASSERT_THAT(IsTrue(Settings.MotionSnap.IsValid() && Settings.MotionSmear.IsValid() && Settings.MotionSettle.IsValid()));
			ASSERT_THAT(IsTrue(FMath::IsNearlyZero(VeyraHudEase::Evaluate(Settings.MotionSnap, 0.0)) && FMath::IsNearlyEqual(VeyraHudEase::Evaluate(Settings.MotionSnap, 1.0), 1.0)));
			ASSERT_THAT(IsTrue(VeyraHudEase::Evaluate(Settings.MotionSnap, 0.25) > 0.5));
			// The smear starts slow; the settle overshoots and comes back.
			ASSERT_THAT(IsTrue(VeyraHudEase::Evaluate(Settings.MotionSmear, 0.25) < 0.25));
			double Highest = 0.0;
			for (double Time = 0.0; Time <= 1.0; Time += 0.05)
			{
				Highest = FMath::Max(Highest, VeyraHudEase::Evaluate(Settings.MotionSettle, Time));
			}
			ASSERT_THAT(IsTrue(Highest > 1.0, TEXT("the settle overshoots")));
			// A straight line is its own time, and time outside the run is clamped.
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraHudEase::Evaluate(Linear(), 0.3), 0.3, 1.0e-4) && VeyraHudEase::Evaluate(Linear(), 2.0) == 1.0));
			ASSERT_THAT(IsTrue(VeyraHudEase::Progress(0.5, 0.0) == 1.0 && VeyraHudEase::Progress(0.25, 1.0) == 0.25));
		}

		TEST_METHOD(AMomentFiresOnlyWhenSomethingTurnsOn)
		{
			FVeyraHudMotion Motion;
			Motion.Begin(Start, false);
			// Already on when first seen: nothing plays.
			ASSERT_THAT(IsTrue(Motion.OnRise(TEXT("Ready"), true) >= FVeyraHudMotion::LongAgo));
			Motion.Begin(Start + Frame, false);
			ASSERT_THAT(IsTrue(Motion.OnRise(TEXT("Ready"), false) >= FVeyraHudMotion::LongAgo));
			Motion.Begin(Start + 2.0 * Frame, false);
			ASSERT_THAT(IsTrue(FMath::IsNearlyZero(Motion.OnRise(TEXT("Ready"), true)), TEXT("it fires as it turns on")));
			Motion.Begin(Start + 3.0 * Frame, false);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Motion.OnRise(TEXT("Ready"), true), Frame), TEXT("and ages after")));
			// Under Reduce UI Animation it has always long ended.
			Motion.Begin(Start + 4.0 * Frame, true);
			ASSERT_THAT(IsTrue(Motion.SinceFired(TEXT("Ready")) >= FVeyraHudMotion::LongAgo));
		}

		TEST_METHOD(HeldStartsAgainForANewIdentity)
		{
			FVeyraHudMotion Motion;
			Motion.Begin(Start, false);
			ASSERT_THAT(IsTrue(Motion.Held(TEXT("Tooltip"), false) < 0.0));
			ASSERT_THAT(IsTrue(FMath::IsNearlyZero(Motion.Held(TEXT("Tooltip"), true, 1))));
			Motion.Begin(Start + 0.1, false);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Motion.Held(TEXT("Tooltip"), true, 1), 0.1)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyZero(Motion.Held(TEXT("Tooltip"), true, 2)), TEXT("another tile's tooltip opens anew")));
		}

		TEST_METHOD(ATrailHoldsAFallThenDrainsAndFollowsARiseAtOnce)
		{
			FVeyraHudMotion Motion;
			Motion.Begin(Start, false);
			ASSERT_THAT(IsTrue(Motion.Trail(TEXT("Health"), 1.0, Hold, Drain, Linear()) == 1.0));
			// A hit: the trail stays where it was through the hold.
			Motion.Begin(Start + Frame, false);
			ASSERT_THAT(IsTrue(Motion.Trail(TEXT("Health"), 0.6, Hold, Drain, Linear()) == 1.0));
			Motion.Begin(Start + Frame + Hold / 2.0, false);
			ASSERT_THAT(IsTrue(Motion.Trail(TEXT("Health"), 0.6, Hold, Drain, Linear()) == 1.0));
			// Half through the drain, half way down; then at the value.
			Motion.Begin(Start + Frame + Hold + Drain / 2.0, false);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Motion.Trail(TEXT("Health"), 0.6, Hold, Drain, Linear()), 0.8, 1.0e-3)));
			Motion.Begin(Start + Frame + Hold + Drain * 2.0, false);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Motion.Trail(TEXT("Health"), 0.6, Hold, Drain, Linear()), 0.6)));
			// A heal is followed at once.
			Motion.Begin(Start + 2.0, false);
			ASSERT_THAT(IsTrue(Motion.Trail(TEXT("Health"), 0.9, Hold, Drain, Linear()) == 0.9));
			// Under Reduce UI Animation the trail is the value.
			Motion.Begin(Start + 2.0 + Frame, true);
			ASSERT_THAT(IsTrue(Motion.Trail(TEXT("Health"), 0.2, Hold, Drain, Linear()) == 0.2));
		}

		TEST_METHOD(ACountRunsToItsNewValueAndSaysItRose)
		{
			FVeyraHudMotion Motion;
			constexpr double Seconds = 0.5;
			Motion.Begin(Start, false);
			ASSERT_THAT(IsTrue(Motion.Count(TEXT("Gold"), 500.0, Seconds, Linear()).Shown == 500.0));
			Motion.Begin(Start + Frame, false);
			const FVeyraHudMotion::FCount Earned = Motion.Count(TEXT("Gold"), 800.0, Seconds, Linear());
			ASSERT_THAT(IsTrue(Earned.Shown == 500.0 && Earned.bRose && FMath::IsNearlyZero(Earned.SinceChange)));
			Motion.Begin(Start + Frame + Seconds / 2.0, false);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Motion.Count(TEXT("Gold"), 800.0, Seconds, Linear()).Shown, 650.0, 0.5)));
			Motion.Begin(Start + Frame + Seconds, false);
			const FVeyraHudMotion::FCount Spent = Motion.Count(TEXT("Gold"), 300.0, Seconds, Linear());
			ASSERT_THAT(IsTrue(Spent.Shown == 800.0 && !Spent.bRose));
		}

		TEST_METHOD(EveryStatTheHudShowsHasItsIconAndColour)
		{
			// ADR-082 §4: the stats panel draws each stat's icon in its colour.
			const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
			for (const TCHAR* Stat : { TEXT("physical_power"), TEXT("magic_power"), TEXT("armor"), TEXT("magic_resist"), TEXT("attack_speed"), TEXT("ability_haste"),
					 TEXT("crit_chance"), TEXT("move_speed"), TEXT("health_regen"), TEXT("resource_regen"), TEXT("physical_penetration"), TEXT("magic_penetration"),
					 TEXT("tenacity"), TEXT("attack_range") })
			{
				const TArray<TArray<FVector2D>> Strokes = VeyraStatGlyphs::Of(Stat);
				ASSERT_THAT(IsTrue(!Strokes.IsEmpty() && Settings.StatColors.Contains(Stat), Stat));
				for (const TArray<FVector2D>& Stroke : Strokes)
				{
					for (const FVector2D& Point : Stroke)
					{
						ASSERT_THAT(IsTrue(Point.X >= 0.0 && Point.Y >= 0.0 && Point.X <= VeyraStatGlyphs::Size && Point.Y <= VeyraStatGlyphs::Size, Stat));
					}
				}
			}
		}

		TEST_METHOD(AKeyNoFrameAsksForIsForgotten)
		{
			FVeyraHudMotion Motion;
			Motion.Begin(Start, false);
			Motion.Fire(TEXT("Gone"));
			Motion.OnRise(TEXT("Kept"), true);
			for (double At = Start; At < Start + 5.0; At += 0.5)
			{
				Motion.Begin(At, false);
				Motion.OnRise(TEXT("Kept"), true);
			}
			ASSERT_THAT(AreEqual(Motion.Num(), 1));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI
