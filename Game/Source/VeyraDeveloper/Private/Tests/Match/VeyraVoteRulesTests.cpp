// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Tuning/VeyraMatchTuning.h"
#include "Votes/VeyraVoteRules.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVoteRulesTests
{
	// Veyra.Match.VoteRules.*: remake, surrender and pause votes, row by row (Match Flow Bible §7–§10;
	// ADR-019 §4). Fixture values, the bible's.
	TEST_CLASS(VoteRules, "Veyra.Match")
	{
		FVeyraVotesTuning Tuning;
		FVeyraVoteCooldowns Cooldowns;

		BEFORE_EACH()
		{
			Tuning.Remake.StartBeforeSeconds = 300.0;
			Tuning.Remake.YesVotes = 3;
			Tuning.Remake.WindowSeconds = 30.0;
			Tuning.Remake.CooldownSeconds = 60.0;
			Tuning.Surrender.StartAfterSeconds = 900.0;
			Tuning.Surrender.YesVotes = 3;
			Tuning.Surrender.WindowSeconds = 30.0;
			Tuning.Surrender.CooldownSeconds = 180.0;
			Tuning.Pause.WindowSeconds = 60.0;
			Tuning.Pause.CooldownSeconds = 180.0;
			Tuning.Pause.IntermissionSeconds = 600.0;
		}

		VeyraVotes::FStartContext Context(EVeyraVoteKind Kind, double Clock, bool bPaused = false) const
		{
			VeyraVotes::FStartContext Out;
			Out.Kind = Kind;
			Out.Team = EVeyraTeam::A;
			Out.MatchClock = Clock;
			Out.Now = 1000.0;
			Out.bVotes = true;
			Out.bMatchmade = true;
			Out.bLive = true;
			Out.bPaused = bPaused;
			return Out;
		}

		static TArray<FVeyraVoter> FiveAgainstFive()
		{
			TArray<FVeyraVoter> Voters;
			for (int32 Index = 0; Index < 10; ++Index)
			{
				Voters.Add({ Index, Index < 5 ? EVeyraTeam::A : EVeyraTeam::B });
			}
			return Voters;
		}

		TEST_METHOD(EachVoteStartsOnlyInItsWindow)
		{
			ASSERT_THAT(IsTrue(VeyraVotes::CheckStart(Context(EVeyraVoteKind::Remake, 0.0), Cooldowns, Tuning) == EVeyraVoteRefusal::None, TEXT("remake from 0:00")));
			ASSERT_THAT(IsTrue(VeyraVotes::CheckStart(Context(EVeyraVoteKind::Remake, 301.0), Cooldowns, Tuning) == EVeyraVoteRefusal::TooLate, TEXT("not after 5:00")));
			ASSERT_THAT(IsTrue(VeyraVotes::CheckStart(Context(EVeyraVoteKind::Surrender, 899.0), Cooldowns, Tuning) == EVeyraVoteRefusal::TooEarly, TEXT("surrender from 15:00")));
			ASSERT_THAT(IsTrue(VeyraVotes::CheckStart(Context(EVeyraVoteKind::Surrender, 900.0), Cooldowns, Tuning) == EVeyraVoteRefusal::None));
			ASSERT_THAT(IsTrue(VeyraVotes::CheckStart(Context(EVeyraVoteKind::Pause, 1.0, true), Cooldowns, Tuning) == EVeyraVoteRefusal::AlreadyPaused));
			ASSERT_THAT(IsTrue(VeyraVotes::CheckStart(Context(EVeyraVoteKind::Resume, 1.0), Cooldowns, Tuning) == EVeyraVoteRefusal::NotPaused));
			ASSERT_THAT(IsTrue(VeyraVotes::CheckStart(Context(EVeyraVoteKind::Resume, 1.0, true), Cooldowns, Tuning) == EVeyraVoteRefusal::None));
			VeyraVotes::FStartContext Practice = Context(EVeyraVoteKind::Pause, 1.0);
			Practice.bVotes = Practice.bMatchmade = false;
			ASSERT_THAT(IsTrue(VeyraVotes::CheckStart(Practice, Cooldowns, Tuning) == EVeyraVoteRefusal::NotStandard, TEXT("a practice match's host ends it")));
			// A custom match that can be won takes surrender votes, not remake or pause (ADR-021 §3).
			VeyraVotes::FStartContext Custom = Context(EVeyraVoteKind::Surrender, 900.0);
			Custom.bMatchmade = false;
			ASSERT_THAT(IsTrue(VeyraVotes::CheckStart(Custom, Cooldowns, Tuning) == EVeyraVoteRefusal::None, TEXT("a custom surrender")));
			Custom.Kind = EVeyraVoteKind::Remake;
			Custom.MatchClock = 0.0;
			ASSERT_THAT(IsTrue(VeyraVotes::CheckStart(Custom, Cooldowns, Tuning) == EVeyraVoteRefusal::NotStandard, TEXT("no custom remake")));
			Custom.Kind = EVeyraVoteKind::Pause;
			ASSERT_THAT(IsTrue(VeyraVotes::CheckStart(Custom, Cooldowns, Tuning) == EVeyraVoteRefusal::NotStandard, TEXT("no custom pause")));
			VeyraVotes::FStartContext Busy = Context(EVeyraVoteKind::Pause, 1.0);
			Busy.bVoteOpen = true;
			ASSERT_THAT(IsTrue(VeyraVotes::CheckStart(Busy, Cooldowns, Tuning) == EVeyraVoteRefusal::AnotherVote, TEXT("one vote at a time")));
		}

		TEST_METHOD(AFailedVoteWaitsItsCooldownForItsTeamOnly)
		{
			FVeyraBallotBox Box;
			Box.Kind = EVeyraVoteKind::Surrender;
			Box.Team = EVeyraTeam::A;
			VeyraVotes::NoteFailed(Box, 1000.0, Tuning, Cooldowns);
			VeyraVotes::FStartContext Later = Context(EVeyraVoteKind::Surrender, 1000.0);
			Later.Now = 1000.0 + 179.0;
			ASSERT_THAT(IsTrue(VeyraVotes::CheckStart(Later, Cooldowns, Tuning) == EVeyraVoteRefusal::CoolingDown));
			Later.Team = EVeyraTeam::B;
			ASSERT_THAT(IsTrue(VeyraVotes::CheckStart(Later, Cooldowns, Tuning) == EVeyraVoteRefusal::None, TEXT("the other team keeps its own")));
			Later.Team = EVeyraTeam::A;
			Later.Now = 1000.0 + 180.0;
			ASSERT_THAT(IsTrue(VeyraVotes::CheckStart(Later, Cooldowns, Tuning) == EVeyraVoteRefusal::None));
		}

		TEST_METHOD(ATeamVotePassesOnThreeOfFiveAndFailsOnceItCannot)
		{
			const TArray<FVeyraVoter> Voters = FiveAgainstFive();
			FVeyraBallotBox Box;
			Box.Kind = EVeyraVoteKind::Surrender;
			Box.Team = EVeyraTeam::A;
			Box.EndsAt = 30.0;
			Box.Ballots = { { 0, true }, { 1, true } };
			ASSERT_THAT(IsTrue(VeyraVotes::Tally(Box, Voters, 1.0, Tuning) == EVeyraVoteOutcome::Open));
			Box.Ballots.Add(5, true);
			ASSERT_THAT(IsTrue(VeyraVotes::Tally(Box, Voters, 1.0, Tuning) == EVeyraVoteOutcome::Open, TEXT("the other team does not vote")));
			Box.Ballots.Add(2, true);
			ASSERT_THAT(IsTrue(VeyraVotes::Tally(Box, Voters, 1.0, Tuning) == EVeyraVoteOutcome::Passed));
			Box.Ballots = { { 0, true }, { 1, false }, { 2, false }, { 3, false } };
			ASSERT_THAT(IsTrue(VeyraVotes::Tally(Box, Voters, 1.0, Tuning) == EVeyraVoteOutcome::Failed, TEXT("one undecided cannot make three")));
			Box.Ballots = { { 0, true } };
			ASSERT_THAT(IsTrue(VeyraVotes::Tally(Box, Voters, 30.0, Tuning) == EVeyraVoteOutcome::Failed, TEXT("the window closed")));
		}

		TEST_METHOD(ADisconnectedPlayerVotesYesOnARemakeAndAbstainsOnASurrender)
		{
			FVeyraVoter Away{ 3, EVeyraTeam::A };
			Away.bDisconnected = true;
			ASSERT_THAT(IsTrue(VeyraVotes::AutomaticBallot(EVeyraVoteKind::Remake, Away) == TOptional<bool>(true)));
			ASSERT_THAT(IsFalse(VeyraVotes::AutomaticBallot(EVeyraVoteKind::Surrender, Away).IsSet()));
			ASSERT_THAT(IsTrue(VeyraVotes::AutomaticBallot(EVeyraVoteKind::Pause, Away) == TOptional<bool>(true)));
			FVeyraVoter Idle{ 4, EVeyraTeam::A };
			Idle.bAfk = true;
			ASSERT_THAT(IsTrue(VeyraVotes::AutomaticBallot(EVeyraVoteKind::Resume, Idle) == TOptional<bool>(true)));
			ASSERT_THAT(IsFalse(VeyraVotes::AutomaticBallot(EVeyraVoteKind::Surrender, Idle).IsSet(), TEXT("AFK players still choose on a surrender")));
			FVeyraVoter Bot{ 5, EVeyraTeam::A };
			Bot.bBot = true;
			ASSERT_THAT(IsFalse(VeyraVotes::AutomaticBallot(EVeyraVoteKind::Surrender, Bot).IsSet(), TEXT("a bot abstains on its team's vote")));
			ASSERT_THAT(IsTrue(VeyraVotes::AutomaticBallot(EVeyraVoteKind::Pause, Bot) == TOptional<bool>(true)));

			// Measured against the full team: two present players cannot reach three once three are gone.
			TArray<FVeyraVoter> Voters = FiveAgainstFive();
			for (int32 Index : { 2, 3, 4 })
			{
				Voters[Index].bDisconnected = true;
			}
			FVeyraBallotBox Box;
			Box.Kind = EVeyraVoteKind::Surrender;
			Box.Team = EVeyraTeam::A;
			Box.EndsAt = 30.0;
			Box.Ballots = { { 0, true } };
			ASSERT_THAT(IsTrue(VeyraVotes::Tally(Box, Voters, 1.0, Tuning) == EVeyraVoteOutcome::Failed, TEXT("abstentions leave it short")));
		}

		TEST_METHOD(AnEarlyResumeVoteStaysOpenForTheRestOfTheIntermission)
		{
			const double Now = 100.0;
			const TOptional<double> IntermissionEndsAt = Now + Tuning.Pause.IntermissionSeconds - 50.0;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraVotes::ClosesAt(EVeyraVoteKind::Resume, Now, IntermissionEndsAt, Tuning), IntermissionEndsAt.GetValue()),
				TEXT("not the pause vote's window")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraVotes::ClosesAt(EVeyraVoteKind::Pause, Now, {}, Tuning), Now + Tuning.Pause.WindowSeconds)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraVotes::ClosesAt(EVeyraVoteKind::Surrender, Now, IntermissionEndsAt, Tuning), Now + Tuning.Surrender.WindowSeconds)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraVotes::ClosesAt(EVeyraVoteKind::Remake, Now, {}, Tuning), Now + Tuning.Remake.WindowSeconds)));
		}

		TEST_METHOD(APauseNeedsEveryPlayerAndFailsOnAnyNo)
		{
			const TArray<FVeyraVoter> Voters = FiveAgainstFive();
			FVeyraBallotBox Box;
			Box.Kind = EVeyraVoteKind::Pause;
			Box.EndsAt = 60.0;
			for (int32 Index = 0; Index < 9; ++Index)
			{
				Box.Ballots.Add(Index, true);
			}
			ASSERT_THAT(AreEqual(VeyraVotes::YesNeeded(Box, Voters, Tuning), 10));
			ASSERT_THAT(IsTrue(VeyraVotes::Tally(Box, Voters, 1.0, Tuning) == EVeyraVoteOutcome::Open));
			Box.Ballots.Add(9, false);
			ASSERT_THAT(IsTrue(VeyraVotes::Tally(Box, Voters, 1.0, Tuning) == EVeyraVoteOutcome::Failed));
			Box.Ballots.Add(9, true);
			ASSERT_THAT(IsTrue(VeyraVotes::Tally(Box, Voters, 1.0, Tuning) == EVeyraVoteOutcome::Passed));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
