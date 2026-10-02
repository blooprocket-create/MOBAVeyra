// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Greybox/VeyraGreyboxSettings.h"
#include "Hud/VeyraHudWarnings.h"

namespace VeyraHudWarningsTests
{
	// Veyra.UI.HudWarnings.*: when the connection and frame-rate warnings show (Settings Bible §3.6, Proposal 110;
	// ADR-055 §5). Fixture values, not the committed ones.
	TEST_CLASS(HudWarnings, "Veyra.UI")
	{
		static constexpr double Start = 3.0;
		static constexpr double Clear = 3.0;

		TEST_METHOD(AWarningStartsAfterLastingTroubleAndClearsAfterLastingCalm)
		{
			using namespace VeyraHudWarnings;
			FVeyraWarningState State;
			ASSERT_THAT(IsFalse(Update(State, true, 0.0, Start, Clear), TEXT("not at the first sign")));
			ASSERT_THAT(IsFalse(Update(State, false, 1.0, Start, Clear)));
			ASSERT_THAT(IsFalse(Update(State, true, 2.0, Start, Clear), TEXT("a break in the trouble starts it again")));
			ASSERT_THAT(IsFalse(Update(State, true, 4.9, Start, Clear)));
			ASSERT_THAT(IsTrue(Update(State, true, 5.0, Start, Clear), TEXT("after lasting trouble")));
			ASSERT_THAT(IsTrue(Update(State, false, 6.0, Start, Clear), TEXT("a moment's calm keeps it")));
			ASSERT_THAT(IsTrue(Update(State, true, 6.5, Start, Clear)));
			ASSERT_THAT(IsTrue(Update(State, false, 7.0, Start, Clear)));
			ASSERT_THAT(IsFalse(Update(State, false, 10.0, Start, Clear), TEXT("lasting calm clears it")));
		}

		TEST_METHOD(TroubleIsLossLagOrAFrameRateWellBelowTheCap)
		{
			using namespace VeyraHudWarnings;
			ASSERT_THAT(IsTrue(IsConnectionTroubled(0.06, 50.0, 0.05, 200.0) && IsConnectionTroubled(0.0, 250.0, 0.05, 200.0)));
			ASSERT_THAT(IsFalse(IsConnectionTroubled(0.05, 200.0, 0.05, 200.0), TEXT("at the thresholds, still fine")));
			ASSERT_THAT(IsTrue(IsPerformanceTroubled(true, 40.0, 60.0, 60.0, 0.7)));
			ASSERT_THAT(IsFalse(IsPerformanceTroubled(true, 50.0, 60.0, 60.0, 0.7)));
			ASSERT_THAT(IsFalse(IsPerformanceTroubled(false, 10.0, 60.0, 60.0, 0.7), TEXT("never from the background cap")));
			ASSERT_THAT(IsTrue(IsPerformanceTroubled(true, 30.0, 0.0, 60.0, 0.7), TEXT("uncapped: against the reference")));
		}

		TEST_METHOD(TheCommittedThresholdsAreUsable)
		{
			const UVeyraGreyboxSettings& Hud = *GetDefault<UVeyraGreyboxSettings>();
			ASSERT_THAT(IsTrue(Hud.ConnectionWarningLossFraction > 0.0f && Hud.ConnectionWarningRoundTripMs > 0.0f && Hud.PerformanceWarningFraction > 0.0f
				&& Hud.PerformanceWarningFraction < 1.0f && Hud.WarningStartSeconds > 0.0f && Hud.WarningClearSeconds > 0.0f && Hud.UncappedReferenceFps > 0.0f));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI