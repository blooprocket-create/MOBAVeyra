// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Chat/VeyraChatRules.h"
#include "CQTest.h"
#include "Tuning/VeyraMatchTuning.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraChatRulesTests
{
	// Veyra.Match.ChatRules.*: what may be sent, how often, and who receives it (Chat & Communication
	// Bible §2; ADR-029). Fixture values, not the committed tuning.
	TEST_CLASS(ChatRules, "Veyra.Match")
	{
		FVeyraChatTuning Tuning;

		BEFORE_EACH()
		{
			Tuning.MaxCharacters = 10;
			Tuning.MaxPerWindow = 2;
			Tuning.WindowSeconds = 5.0;
			Tuning.KeepMessages = 3;
		}

		TEST_METHOD(TextIsCleanedThenMustBeSomethingAndShortEnough)
		{
			ASSERT_THAT(AreEqual(FString(TEXT("go mid")), VeyraChat::Clean(TEXT("  go\tmid\n "))));
			ASSERT_THAT(IsTrue(VeyraChat::CheckText(VeyraChat::Clean(TEXT(" \t\r\n ")), Tuning) == EVeyraChatRefusal::Empty));
			ASSERT_THAT(IsTrue(VeyraChat::CheckText(TEXT("0123456789"), Tuning) == EVeyraChatRefusal::None, TEXT("its most characters")));
			ASSERT_THAT(IsTrue(VeyraChat::CheckText(TEXT("0123456789A"), Tuning) == EVeyraChatRefusal::TooLong));
		}

		TEST_METHOD(APlayerMayChatAFewTimesInAWindowThenWaits)
		{
			TArray<double> SentAt;
			ASSERT_THAT(IsTrue(VeyraChat::Allow(SentAt, 0.0, Tuning) && VeyraChat::Allow(SentAt, 1.0, Tuning)));
			ASSERT_THAT(IsFalse(VeyraChat::Allow(SentAt, 4.9, Tuning), TEXT("a third inside the window")));
			ASSERT_THAT(IsTrue(VeyraChat::Allow(SentAt, 5.0, Tuning), TEXT("the first has left the window")));
		}

		TEST_METHOD(TeamReachesTheSideAllReachesWhoKeepsItOnAndMutingReachesNoOne)
		{
			using namespace VeyraChat;
			ASSERT_THAT(IsTrue(Receives(EVeyraChatChannel::Team, EVeyraTeam::A, EVeyraTeam::A, false, false), TEXT("an ally, All Chat off or not")));
			ASSERT_THAT(IsFalse(Receives(EVeyraChatChannel::Team, EVeyraTeam::A, EVeyraTeam::B, true, false), TEXT("never an enemy")));
			ASSERT_THAT(IsTrue(Receives(EVeyraChatChannel::All, EVeyraTeam::A, EVeyraTeam::B, true, false), TEXT("All reaches the other side")));
			ASSERT_THAT(IsFalse(Receives(EVeyraChatChannel::All, EVeyraTeam::A, EVeyraTeam::A, false, false), TEXT("not a reader with All Chat off")));
			ASSERT_THAT(IsFalse(Receives(EVeyraChatChannel::Team, EVeyraTeam::A, EVeyraTeam::A, true, true), TEXT("nor one who muted the sender")));
		}

		TEST_METHOD(AClientKeepsItsNewestMessages)
		{
			TArray<FVeyraReceivedChat> Held;
			for (int32 Index = 0; Index < Tuning.KeepMessages + 2; ++Index)
			{
				FVeyraReceivedChat& Received = Held.AddDefaulted_GetRef();
				Received.Message.Text = FString::FromInt(Index);
			}
			VeyraChat::Forget(Held, Tuning);
			ASSERT_THAT(AreEqual(Tuning.KeepMessages, Held.Num()));
			ASSERT_THAT(AreEqual(FString(TEXT("2")), Held[0].Message.Text, TEXT("the oldest go")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
