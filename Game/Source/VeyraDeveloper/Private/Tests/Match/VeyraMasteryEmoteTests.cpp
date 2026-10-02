// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER

#include "Emote/VeyraMasteryEmote.h"
#include "Tuning/VeyraMatchTuning.h"
#include "Tuning/VeyraMatchTuningSubsystem.h"

namespace VeyraMasteryEmoteTests
{
	// Veyra.Match.MasteryEmoteRule.*: who may show the mastery emote, and how often (ADR-045 §9).
	TEST_CLASS(MasteryEmoteRule, "Veyra.Match")
	{
		// Fixture values, independent of the committed tuning.
		static constexpr double EmoteSeconds = 3.0;
		static constexpr double CooldownSeconds = 10.0;
		static constexpr double Now = 100.0;

		static FVeyraMasteryEmoteTuning Fixture()
		{
			FVeyraMasteryEmoteTuning Tuning;
			Tuning.Seconds = EmoteSeconds;
			Tuning.CooldownSeconds = CooldownSeconds;
			return Tuning;
		}

		TEST_METHOD(APlayerWithMasteryShowsItForItsLength)
		{
			const TOptional<double> Until = VeyraMasteryEmote::Show(4, Now, 0.0, Fixture());
			ASSERT_THAT(IsTrue(Until.IsSet() && FMath::IsNearlyEqual(Until.GetValue(), Now + EmoteSeconds)));
			ASSERT_THAT(IsTrue(VeyraMasteryEmote::Show(4, Now, Now, Fixture()).IsSet(), TEXT("the cooldown ends at its moment")));
		}

		TEST_METHOD(NoMasteryOrTheCooldownRefusesIt)
		{
			ASSERT_THAT(IsFalse(VeyraMasteryEmote::Show(0, Now, 0.0, Fixture()).IsSet(), TEXT("a bot, or a match without progression")));
			ASSERT_THAT(IsFalse(VeyraMasteryEmote::Show(4, Now, Now + 1.0, Fixture()).IsSet(), TEXT("within the cooldown")));
		}

		TEST_METHOD(TheCommittedTuningGivesTheEmoteALengthAndACooldown)
		{
			const FVeyraMasteryEmoteTuning& Committed = UVeyraMatchTuningSubsystem::Get().MasteryEmote;
			ASSERT_THAT(IsTrue(Committed.Seconds > 0.0 && Committed.CooldownSeconds >= Committed.Seconds));
		}
	};
}

#endif
