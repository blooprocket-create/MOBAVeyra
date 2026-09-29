// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "Join/VeyraMatchHostSubsystem.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "VeyraPlayerState.h"

namespace VeyraNetTests
{
	// Veyra.Net.Votes.*: players start and answer votes through their controllers, every player sees
	// the open vote, and what passes happens (Match Flow Bible §7–§10; ADR-019 §4). One player per
	// side, so the fixture's team votes pass on one YES.
	NETWORK_TEST_CLASS(Votes, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedMatchTuning> Tuning;
		TUniquePtr<FScopedTestTickets> Tickets;
		TUniquePtr<FScopedMatchAssignment> Assignment;
		FVeyraGreyboxLayout Layout;
		TOptional<FVeyraMatchResult> Result;
		FDelegateHandle EndedHandle;

		// Fixture values.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr int32 OneVote = 1;
		static constexpr double ShortIntermissionSeconds = 1.0;
		static constexpr double LongIntermissionSeconds = 600.0;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			Tuning->Tuning.Votes.Remake.YesVotes = OneVote;
			Tuning->Tuning.Votes.Surrender.YesVotes = OneVote;
			Tuning->Tuning.Votes.Surrender.StartAfterSeconds = 0.0;
			Tuning->Tuning.Votes.Pause.IntermissionSeconds = LongIntermissionSeconds;
			Tickets = MakeUnique<FScopedTestTickets>();
			Assignment = MakeUnique<FScopedMatchAssignment>(TArray<EVeyraTeam>{ EVeyraTeam::A, EVeyraTeam::B });
			ASSERT_THAT(IsTrue(Assignment->Problems.IsEmpty(), FString::Join(Assignment->Problems, TEXT(" | "))));
			EndedHandle = UVeyraMatchHostSubsystem::Get()->OnMatchEnded.AddLambda([this](const FVeyraMatchResult& Ended) { Result = Ended; });
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			UVeyraMatchHostSubsystem::Get()->OnMatchEnded.Remove(EndedHandle);
			Assignment.Reset();
			Tickets.Reset();
			Tuning.Reset();
		}

		TEST_METHOD(ASurrenderLosesTheMatchForTheTeamThatVoted)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenClient(TEXT("A player surrenders"), 0, [](FState& State) { LocalControllerOf(State.World)->RequestVote(EVeyraVoteKind::Surrender); })
				.UntilServer(TEXT("The match ends"), [this](FState& /*State*/) { return Result.IsSet(); })
				.ThenServer(TEXT("It ends by surrender, won by the other side"), [this](FState& State) {
					const APlayerState* Surrendered = ServerControllerOf(State, 0)->PlayerState;
					const EVeyraTeam Team = Cast<AVeyraPlayerState>(Surrendered)->GetVeyraTeam();
					ASSERT_THAT(IsTrue(Result->EndReason == EVeyraMatchEndReason::Surrender));
					ASSERT_THAT(IsTrue(Result->Winner == VeyraTeams::Opposing(Team)));
				});
		}

		TEST_METHOD(ARemakeEndsTheMatchAsNoContest)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenClient(TEXT("A player asks for a remake"), 0, [](FState& State) { LocalControllerOf(State.World)->RequestVote(EVeyraVoteKind::Remake); })
				.UntilServer(TEXT("The match ends"), [this](FState& /*State*/) { return Result.IsSet(); })
				.ThenServer(TEXT("It ends as a remake with no winner"), [this](FState& /*State*/) {
					ASSERT_THAT(IsTrue(Result->EndReason == EVeyraMatchEndReason::Remake && Result->Winner == EVeyraTeam::None));
				});
		}

		TEST_METHOD(APauseNeedsEveryPlayerAndAnEarlyResumeToo)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenClient(TEXT("A player asks to pause"), 0, [](FState& State) { LocalControllerOf(State.World)->RequestVote(EVeyraVoteKind::Pause); })
				.UntilClients(TEXT("Every player sees the vote, one YES of two"), [](FState& State) {
					const FVeyraVoteState& Vote = GameStateOf(State.World)->GetVote();
					return Vote.bOpen && Vote.Kind == EVeyraVoteKind::Pause && Vote.Yes == 1 && Vote.Needed == MatchClientCount;
				})
				.ThenServer(TEXT("Play goes on while it is open"), [this](FState& State) { ASSERT_THAT(IsFalse(GameStateOf(State.World)->IsMatchPaused())); })
				.ThenClient(TEXT("The other player agrees"), 1, [](FState& State) { LocalControllerOf(State.World)->CastVote(true); })
				.UntilServer(TEXT("The match pauses for an intermission"), [](FState& State) {
					return GameStateOf(State.World)->IsMatchPaused() && GameStateOf(State.World)->GetIntermissionSecondsLeft() > 0;
				})
				.ThenClient(TEXT("A player asks to resume early"), 1, [](FState& State) { LocalControllerOf(State.World)->RequestVote(EVeyraVoteKind::Resume); })
				.ThenClient(TEXT("The other agrees"), 0, [](FState& State) { LocalControllerOf(State.World)->CastVote(true); })
				.UntilServer(TEXT("Play resumes"), [](FState& State) {
					return !GameStateOf(State.World)->IsMatchPaused() && GameStateOf(State.World)->GetIntermissionSecondsLeft() == 0;
				});
		}

		TEST_METHOD(AnUnansweredIntermissionEndsByItself)
		{
			Tuning->Tuning.Votes.Pause.IntermissionSeconds = ShortIntermissionSeconds;
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenClient(TEXT("A player asks to pause"), 0, [](FState& State) { LocalControllerOf(State.World)->RequestVote(EVeyraVoteKind::Pause); })
				.ThenClient(TEXT("The other agrees"), 1, [](FState& State) { LocalControllerOf(State.World)->CastVote(true); })
				.UntilServer(TEXT("The match pauses"), [](FState& State) { return GameStateOf(State.World)->IsMatchPaused(); })
				.UntilServer(TEXT("And resumes by itself"), [](FState& State) { return !GameStateOf(State.World)->IsMatchPaused(); });
		}

		TEST_METHOD(ASecondVoteWaitsAndAPracticeTakesNone)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenClient(TEXT("A player asks to pause"), 0, [](FState& State) { LocalControllerOf(State.World)->RequestVote(EVeyraVoteKind::Pause); })
				.ThenClient(TEXT("The other asks to surrender at once"), 1, [](FState& State) { LocalControllerOf(State.World)->RequestVote(EVeyraVoteKind::Surrender); })
				.UntilClient(TEXT("It is refused: one vote at a time"), 1, [](FState& State) {
					return LocalControllerOf(State.World)->GetLastVoteRefusal() == EVeyraVoteRefusal::AnotherVote;
				})
				.ThenClient(TEXT("It answers the open vote twice"), 1, [](FState& State) {
					LocalControllerOf(State.World)->CastVote(false);
					LocalControllerOf(State.World)->CastVote(true);
				})
				.UntilClient(TEXT("The second ballot is refused: a ballot is locked"), 1, [](FState& State) {
					return LocalControllerOf(State.World)->GetLastVoteRefusal() == EVeyraVoteRefusal::NoVote
						|| LocalControllerOf(State.World)->GetLastVoteRefusal() == EVeyraVoteRefusal::AlreadyVoted;
				})
				.ThenServer(TEXT("Its NO failed the pause"), [this](FState& State) {
					ASSERT_THAT(IsFalse(GameStateOf(State.World)->IsMatchPaused()));
					ASSERT_THAT(IsFalse(GameStateOf(State.World)->GetVote().bOpen));
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
