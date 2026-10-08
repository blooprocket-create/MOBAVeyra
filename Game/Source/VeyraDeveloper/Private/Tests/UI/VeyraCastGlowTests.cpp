// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Greybox/VeyraCastGlow.h"

namespace VeyraCastGlowTests
{
	// Veyra.UI.CastGlow.*: a generated body's glow surging while a cast holds it, and easing back after (ADR-072 §5).
	TEST_CLASS(CastGlow, "Veyra.UI")
	{
		static constexpr double Rise = 0.2;
		static constexpr double Fall = 0.5;
		static constexpr double Gain = 3.0;
		static constexpr double Slack = 1e-6;

		TEST_METHOD(ItRisesWhileACastHoldsTheBodyAndFallsAfter)
		{
			double Glow = VeyraCastGlow::Step(0.0, true, Rise / 2.0, Rise, Fall);
			ASSERT_THAT(IsNear(Glow, 0.5, Slack, TEXT("halfway over half its rise")));
			Glow = VeyraCastGlow::Step(Glow, true, Rise, Rise, Fall);
			ASSERT_THAT(IsNear(Glow, 1.0, Slack, TEXT("never past full strain")));
			Glow = VeyraCastGlow::Step(Glow, false, Fall / 2.0, Rise, Fall);
			ASSERT_THAT(IsNear(Glow, 0.5, Slack, TEXT("easing back over its own, slower fall")));
			Glow = VeyraCastGlow::Step(Glow, false, Fall, Rise, Fall);
			ASSERT_THAT(IsNear(Glow, 0.0, Slack, TEXT("and resting at its own glow")));
		}

		TEST_METHOD(AtRestItChangesNothingAndAtFullStrainItMultipliesByItsGain)
		{
			ASSERT_THAT(IsNear(VeyraCastGlow::Multiplier(0.0, Gain), 1.0, Slack));
			ASSERT_THAT(IsNear(VeyraCastGlow::Multiplier(1.0, Gain), Gain, Slack));
			ASSERT_THAT(IsNear(VeyraCastGlow::Multiplier(0.5, Gain), (1.0 + Gain) / 2.0, Slack));
		}
	};
}

#endif
