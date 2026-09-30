// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Pings/VeyraPingRules.h"
#include "Tuning/VeyraMatchTuning.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraPingRulesTests
{
	// Veyra.Match.PingRules.*: how often a player may ping, and how long a client keeps a ping
	// (ADR-020 §2). Fixture values, not the committed tuning.
	TEST_CLASS(PingRules, "Veyra.Match")
	{
		FVeyraPingsTuning Tuning;

		BEFORE_EACH()
		{
			Tuning.MaxPerWindow = 3;
			Tuning.WindowSeconds = 5.0;
			Tuning.KeepSeconds = 8.0;
		}

		TEST_METHOD(APlayerMayPingAFewTimesInAWindowThenWaits)
		{
			TArray<double> SentAt;
			ASSERT_THAT(IsTrue(VeyraPings::Allow(SentAt, 0.0, Tuning)));
			ASSERT_THAT(IsTrue(VeyraPings::Allow(SentAt, 1.0, Tuning)));
			ASSERT_THAT(IsTrue(VeyraPings::Allow(SentAt, 2.0, Tuning)));
			ASSERT_THAT(IsFalse(VeyraPings::Allow(SentAt, 4.9, Tuning), TEXT("a fourth inside the window")));
			ASSERT_THAT(AreEqual(SentAt.Num(), Tuning.MaxPerWindow, TEXT("a refused ping is not counted")));
			ASSERT_THAT(IsTrue(VeyraPings::Allow(SentAt, 5.0, Tuning), TEXT("the first has left the window")));
			ASSERT_THAT(IsFalse(VeyraPings::Allow(SentAt, 5.5, Tuning)));
		}

		TEST_METHOD(AClientKeepsAPingOnlyForAWhile)
		{
			TArray<FVeyraReceivedPing> Held;
			Held.Add({ FVeyraPing(), 0.0 });
			Held.Add({ FVeyraPing(), 3.0 });
			VeyraPings::Forget(Held, 7.9, Tuning);
			ASSERT_THAT(AreEqual(Held.Num(), 2));
			VeyraPings::Forget(Held, 8.0, Tuning);
			ASSERT_THAT(IsTrue(Held.Num() == 1 && Held[0].ReceivedAt == 3.0));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
