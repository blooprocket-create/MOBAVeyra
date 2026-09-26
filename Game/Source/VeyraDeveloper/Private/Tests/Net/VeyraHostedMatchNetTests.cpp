// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "Join/VeyraMatchHostSubsystem.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "Tuning/VeyraTuning.h"
#include "VeyraJoinRules.h"
#include "VeyraPlayerState.h"

namespace VeyraNetTests
{
	// Veyra.Net.HostedMatch.*: a server hosting an assigned match admits its roster by join ticket,
	// puts each participant on its rostered side, and reports the match's result when it ends
	// (ADR-007).
	NETWORK_TEST_CLASS(HostedMatch, "Veyra.Net")
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
		static constexpr double ShortAbandonSeconds = 0.5;

		BEFORE_EACH()
		{
			IgnoreLoginViewTargetRpc(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			Tuning->Tuning.Lifecycle.AbandonAfterSeconds = ShortAbandonSeconds;
			Tickets = MakeUnique<FScopedTestTickets>();
			// Both on Team A: joining the smaller side would split them, so the roster must decide.
			Assignment = MakeUnique<FScopedMatchAssignment>(TArray<EVeyraTeam>{ EVeyraTeam::A, EVeyraTeam::A });
			ASSERT_THAT(IsTrue(Assignment->Problems.IsEmpty(), FString::Join(Assignment->Problems, TEXT(" | "))));
			EndedHandle = UVeyraMatchHostSubsystem::Get()->OnMatchEnded.AddLambda([this](const FVeyraMatchResult& Ended) { Result = Ended; });
			// A client that disconnects returns to the default map in this same process, and starting
			// that standalone world loads MovieSceneCapture, which Iris warns about while the server
			// replicates. Load it before the network starts. A real client leaves from its own process.
			FModuleManager::Get().LoadModule(TEXT("MovieSceneCapture"));
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			UVeyraMatchHostSubsystem::Get()->OnMatchEnded.Remove(EndedHandle);
			Assignment.Reset();
			Tickets.Reset();
			Tuning.Reset();
		}

		static FString TunedOptions()
		{
			return TEXT("?") + VeyraJoinRules::MakeTuningHashOption(VeyraTuning::GetCompositeHash());
		}

		static FString PreLoginRefusal(UWorld* World, const FString& Options)
		{
			FString Error;
			GameModeOf(World)->PreLogin(Options, TEXT("test"), FUniqueNetIdRepl(), Error);
			return Error;
		}

		void ExpectRefusals(int32 Count)
		{
			TestRunner->AddExpectedMessagePlain(TEXT("Refused a connection from test"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, Count);
		}

		TEST_METHOD(ParticipantsTakeTheirRosteredSideAndAccount)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Each player is a rostered participant on Team A"), [this](FState& State) {
					TSet<FString> Accounts;
					for (int32 Client = 0; Client < MatchClientCount; ++Client)
					{
						const AVeyraPlayerState* Player = ServerControllerOf(State, Client)->GetPlayerState<AVeyraPlayerState>();
						const FString AccountId = Player->GetAccountId();
						const FVeyraAssignedParticipant* Rostered = Assignment->Assignment.Participants.FindByPredicate(
							[&AccountId](const FVeyraAssignedParticipant& Candidate) { return Candidate.AccountId == AccountId; });
						ASSERT_THAT(IsNotNull(Rostered));
						ASSERT_THAT(IsTrue(Player->GetVeyraTeam() == EVeyraTeam::A));
						ASSERT_THAT(AreEqual(Player->GetPlayerName(), Rostered->DisplayName));
						Accounts.Add(AccountId);
					}
					ASSERT_THAT(AreEqual(Accounts.Num(), MatchClientCount));
				});
		}

		TEST_METHOD(RefusesMissingUnknownAndConnectedTickets)
		{
			ExpectRefusals(3);
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Refuse each login"), [this](FState& State) {
					ASSERT_THAT(IsTrue(PreLoginRefusal(State.World, TunedOptions()).Contains(TEXT("sent none"))));
					ASSERT_THAT(IsTrue(PreLoginRefusal(State.World, TunedOptions() + TEXT("?") + VeyraJoinRules::MakeTicketOption(TEXT("vjt_unknown")))
						.Contains(TEXT("not valid for this match"))));
					ASSERT_THAT(IsTrue(PreLoginRefusal(State.World, TunedOptions() + TEXT("?") + VeyraJoinRules::MakeTicketOption(TestTicketForPIEInstance(1)))
						.Contains(TEXT("already connected"))));
				});
		}

		TEST_METHOD(ADeveloperEndReportsTheResult)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenClient(TEXT("A client asks to end the match"), 0, [](FState& State) { LocalControllerOf(State.World)->RequestDeveloperEndMatch(); })
				.UntilServer(TEXT("The match ends"), [this](FState& State) {
					return Result.IsSet() && GameStateOf(State.World)->GetPhase() == EVeyraMatchPhase::Ended;
				})
				.UntilClients(TEXT("Every client sees the end"), [](FState& State) { return GameStateOf(State.World)->GetPhase() == EVeyraMatchPhase::Ended; })
				.ThenServer(TEXT("The result names every participant, and the match takes no more orders"), [this](FState& State) {
					ASSERT_THAT(IsTrue(Result->EndReason == EVeyraMatchEndReason::DeveloperRequest));
					ASSERT_THAT(AreEqual(Result->MatchId, Assignment->Assignment.MatchId));
					ASSERT_THAT(IsTrue(Result->Winner == EVeyraTeam::None));
					ASSERT_THAT(IsTrue(Result->DurationSeconds > 0.0));
					ASSERT_THAT(IsTrue(Result->DurationSeconds == GameStateOf(State.World)->GetMatchClockSeconds()));
					ASSERT_THAT(AreEqual(Result->Participants.Num(), MatchClientCount));
					for (const FVeyraParticipantResult& Participant : Result->Participants)
					{
						ASSERT_THAT(IsTrue(Participant.bJoined && Participant.bConnectedAtEnd));
					}
					ASSERT_THAT(IsTrue(GameModeOf(State.World)->CheckOrdersAllowed() == EVeyraOrderRejection::WrongPhase));
				});
		}

		TEST_METHOD(AMatchEveryoneLeftEndsAsAbandoned)
		{
			ExpectRefusals(1);
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenClients(TEXT("Every client leaves"), [](FState& State) { LocalControllerOf(State.World)->ConsoleCommand(TEXT("disconnect")); })
				.UntilServer(TEXT("The match ends as abandoned"), [this](FState& /*State*/) { return Result.IsSet(); })
				.ThenServer(TEXT("Nobody was connected at the end, and nobody may join an ended match"), [this](FState& State) {
					ASSERT_THAT(IsTrue(Result->EndReason == EVeyraMatchEndReason::Abandoned));
					for (const FVeyraParticipantResult& Participant : Result->Participants)
					{
						ASSERT_THAT(IsTrue(Participant.bJoined && !Participant.bConnectedAtEnd));
					}
					const FString Rejoin = TunedOptions() + TEXT("?") + VeyraJoinRules::MakeTicketOption(TestTicketForPIEInstance(1));
					ASSERT_THAT(IsTrue(PreLoginRefusal(State.World, Rejoin).Contains(TEXT("has ended"))));
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
