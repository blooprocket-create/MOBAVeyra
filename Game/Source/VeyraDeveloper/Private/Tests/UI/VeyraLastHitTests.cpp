// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Hud/VeyraLastHit.h"

namespace VeyraLastHitTests
{
	// Veyra.UI.LastHit.*: the last-hit cue's two stages (ADR-065 §6, ADR-071 §5), with fixture numbers.
	TEST_CLASS(LastHit, "Veyra.UI")
	{
		static constexpr double Hit = 60.0;
		static constexpr double LeadSeconds = 0.8;
		static constexpr double LossPerSecond = 50.0;
		static constexpr double Slack = 1e-6;

		TEST_METHOD(AtOrBelowOneHitAnAttackKillsNow)
		{
			ASSERT_THAT(IsTrue(VeyraLastHit::StageOf(Hit, Hit, 0.0, LeadSeconds) == EVeyraLastHitStage::Now));
			ASSERT_THAT(IsTrue(VeyraLastHit::StageOf(Hit / 2.0, Hit, LossPerSecond, LeadSeconds) == EVeyraLastHitStage::Now));
		}

		TEST_METHOD(ItReadsReadyWhileItWouldFallIntoReachAsTheAttackLands)
		{
			// Losing 50 a second, it loses 40 in an attack's 0.8 seconds: from 100 Health, a 60 hit lands as it reaches 60.
			const double Falls = LossPerSecond * LeadSeconds;
			ASSERT_THAT(IsTrue(VeyraLastHit::StageOf(Hit + Falls, Hit, LossPerSecond, LeadSeconds) == EVeyraLastHitStage::Ready));
			ASSERT_THAT(IsTrue(VeyraLastHit::StageOf(Hit + Falls + 1.0, Hit, LossPerSecond, LeadSeconds) == EVeyraLastHitStage::None,
				TEXT("just above that, it is not yet worth starting")));
		}

		TEST_METHOD(AUnitNothingElseHitsGoesStraightToNow)
		{
			ASSERT_THAT(IsTrue(VeyraLastHit::StageOf(Hit + 1.0, Hit, 0.0, LeadSeconds) == EVeyraLastHitStage::None));
			ASSERT_THAT(IsTrue(VeyraLastHit::StageOf(Hit + 1.0, 0.0, LossPerSecond, LeadSeconds) == EVeyraLastHitStage::None,
				TEXT("an attack that takes nothing shows nothing")));
		}

		TEST_METHOD(AnAttackLandsAfterItsWindupAndItsFlight)
		{
			ASSERT_THAT(IsNear(VeyraLastHit::LeadSecondsOf(0.4, 600.0, 1500.0), 0.8, Slack));
			ASSERT_THAT(IsNear(VeyraLastHit::LeadSecondsOf(0.4, 600.0, 0.0), 0.4, Slack, TEXT("melee lands as its windup ends")));
		}

		TEST_METHOD(TheLossRateCountsOnlyItsWindow)
		{
			FVeyraHealthLoss Loss;
			constexpr double Window = 1.0;
			Loss.Note(0.0, 100.0, Window);
			Loss.Note(1.5, 20.0, Window);
			Loss.Note(1.8, 30.0, Window);
			ASSERT_THAT(IsNear(Loss.PerSecond(2.0, Window), 50.0, Slack, TEXT("the hit two seconds ago is out of the window")));
			ASSERT_THAT(IsNear(Loss.PerSecond(4.0, Window), 0.0, Slack, TEXT("nothing lately, no loss")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI
