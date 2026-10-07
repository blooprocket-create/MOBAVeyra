// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Greybox/VeyraHitFeel.h"

namespace VeyraHitFeelTests
{
	// Veyra.UI.HitFeel.*: the player's own camera kicks on a heavy hit or a fall, as hard as Screen Shake allows (ADR-068 §4).
	TEST_CLASS(HitFeel, "Veyra.UI")
	{
		// Fixture values: a Vanguard's Max Health, the heavy share, the kicks, and the shudder's length and turns.
		static constexpr double MaxHealth = 1000.0;
		static constexpr double Heavy = 0.1;
		static constexpr double Hit = 6.0;
		static constexpr double Fall = 12.0;
		static constexpr double Seconds = 0.2;
		static constexpr double Frequency = 20.0;

		TEST_METHOD(OnlyAHeavyHitOrAFallKicksAndScreenShakeScalesIt)
		{
			using namespace VeyraHitFeel;
			ASSERT_THAT(IsNear(AmplitudeOf(EVeyraCombatCueKind::Hit, MaxHealth * Heavy * 0.5, MaxHealth, Heavy, Hit, Fall, 1.0), 0.0, 1e-9, TEXT("a light hit")));
			ASSERT_THAT(IsNear(AmplitudeOf(EVeyraCombatCueKind::Hit, MaxHealth * Heavy, MaxHealth, Heavy, Hit, Fall, 1.0), Hit, 1e-9, TEXT("a heavy hit")));
			ASSERT_THAT(IsNear(AmplitudeOf(EVeyraCombatCueKind::Death, 0.0, MaxHealth, Heavy, Hit, Fall, 1.0), Fall, 1e-9, TEXT("a fall")));
			ASSERT_THAT(IsNear(AmplitudeOf(EVeyraCombatCueKind::CastCommit, MaxHealth, MaxHealth, Heavy, Hit, Fall, 1.0), 0.0, 1e-9, TEXT("not a cast")));
			ASSERT_THAT(IsNear(AmplitudeOf(EVeyraCombatCueKind::Death, 0.0, MaxHealth, Heavy, Hit, Fall, 0.4), Fall * 0.4, 1e-9, TEXT("Reduced")));
			ASSERT_THAT(IsNear(AmplitudeOf(EVeyraCombatCueKind::Death, 0.0, MaxHealth, Heavy, Hit, Fall, 0.0), 0.0, 1e-9, TEXT("Off")));
		}

		TEST_METHOD(TheShudderFadesToNothingAndThenStill)
		{
			using namespace VeyraHitFeel;
			ASSERT_THAT(IsNear(OffsetAt(Hit, 0.0, Seconds, Frequency).Size(), Hit, 1e-6, TEXT("at its start, the whole kick")));
			ASSERT_THAT(IsTrue(OffsetAt(Hit, Seconds / 2.0, Seconds, Frequency).Size() < Hit, TEXT("then fading")));
			ASSERT_THAT(IsTrue(OffsetAt(Hit, Seconds, Seconds, Frequency).IsZero() && OffsetAt(Hit, -Frame, Seconds, Frequency).IsZero(), TEXT("and still outside it")));
			ASSERT_THAT(IsNear(OffsetAt(Hit, Seconds / 3.0, Seconds, Frequency).X, 0.0, 1e-9, TEXT("across and up the view, never along it")));
			// A fresh blow replaces a fading kick only once it would move the camera more.
			ASSERT_THAT(IsTrue(Outshakes(Fall, 0.0, Seconds, Hit)));
			ASSERT_THAT(IsFalse(Outshakes(Fall, Seconds * 0.9, Seconds, Hit)));
		}

		static constexpr double Frame = 0.016;
	};
}

#endif
