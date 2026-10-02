// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER

#include "Tests/Services/VeyraClientFlowTestRig.h"

namespace VeyraClientFlowTests
{
	// Veyra.Services.ClientFlow.*: the client-state coordinator (ADR-010 §2), driven through a fake
	// backend and engine exactly as the game's presentation and the backend drive it.
	TEST_CLASS(ClientFlow, "Veyra.Services")
	{
		FClientFlowTestRig Rig;
		FFlowTestBackend& Backend = Rig.Backend;
		FFlowTestHost& Host = Rig.Host;
		TUniquePtr<FVeyraClientFlow>& Flow = Rig.Flow;

		EVeyraClientState State() const { return Rig.State(); }
		void Advance(double Seconds) { Rig.Advance(Seconds); }
		bool SignIn() { return Rig.SignIn(); }
		bool ReachShell() { return Rig.ReachShell(); }
		bool ReachSelect() { return Rig.ReachSelect(); }
		bool ReachMatch() { return Rig.ReachMatch(); }
		bool ReachMatchFound() { return Rig.ReachMatchFound(); }

		/** Answers the reads that find where the player is, when that is the shell. */
		bool ResumeToShell()
		{
			return State() == EVeyraClientState::Loading && Backend.Answer(TEXT("GET"), TEXT("/v1/me/match"), 200, NoMatch)
				&& Backend.Answer(TEXT("GET"), TEXT("/v1/me/select"), 200, NoSelect) && Backend.Answer(TEXT("GET"), TEXT("/v1/me/profile"), 200, ProfileBody(true))
				&& Backend.Answer(TEXT("GET"), TEXT("/v1/lobby"), 200, NoLobby) && State() == EVeyraClientState::Shell;
		}

		TEST_METHOD(Transitions)
		{
			// Each intent belongs to one state; Retry and Quit are open to every state, subject to the snapshot.
			const TPair<EVeyraClientIntent, EVeyraClientState> Owners[] = {
				{ EVeyraClientIntent::ChooseStarter, EVeyraClientState::StarterChoice },
				{ EVeyraClientIntent::StartPractice, EVeyraClientState::Shell },
				{ EVeyraClientIntent::SelectMode, EVeyraClientState::Shell },
				{ EVeyraClientIntent::SetReady, EVeyraClientState::Shell },
				{ EVeyraClientIntent::FindMatch, EVeyraClientState::Shell },
				{ EVeyraClientIntent::CancelQueue, EVeyraClientState::Shell },
				{ EVeyraClientIntent::AcceptMatch, EVeyraClientState::MatchFound },
				{ EVeyraClientIntent::DeclineMatch, EVeyraClientState::MatchFound },
				{ EVeyraClientIntent::HoverVanguard, EVeyraClientState::Selecting },
				{ EVeyraClientIntent::LockVanguard, EVeyraClientState::Selecting },
				{ EVeyraClientIntent::LeaveSelect, EVeyraClientState::Selecting },
				{ EVeyraClientIntent::Reconnect, EVeyraClientState::ReconnectOnly },
				{ EVeyraClientIntent::LeaveMatch, EVeyraClientState::InMatch },
				{ EVeyraClientIntent::ContinueFromResults, EVeyraClientState::Results },
				{ EVeyraClientIntent::CreateLobby, EVeyraClientState::Shell },
				{ EVeyraClientIntent::AcceptLobbyInvite, EVeyraClientState::Shell },
				{ EVeyraClientIntent::InviteToLobby, EVeyraClientState::Lobby },
				{ EVeyraClientIntent::LeaveLobby, EVeyraClientState::Lobby },
				{ EVeyraClientIntent::KickFromLobby, EVeyraClientState::Lobby },
				{ EVeyraClientIntent::MoveInLobby, EVeyraClientState::Lobby },
				{ EVeyraClientIntent::SetLobbyBot, EVeyraClientState::Lobby },
				{ EVeyraClientIntent::RemoveLobbyBot, EVeyraClientState::Lobby },
				{ EVeyraClientIntent::SetLobbySettings, EVeyraClientState::Lobby },
				{ EVeyraClientIntent::LaunchLobby, EVeyraClientState::Lobby },
			};
			// The friends panel's intents belong to the shell and the lobby alike; the results screen's player menu
			// also sends friend requests (UX-57), which Veyra.Services.ConductFlow covers.
			const EVeyraClientIntent Social[] = { EVeyraClientIntent::DeclineLobbyInvite, EVeyraClientIntent::AnswerFriendRequest,
				EVeyraClientIntent::RemoveFriend };
			for (uint8 Index = 0; Index <= static_cast<uint8>(EVeyraClientState::SessionEnded); ++Index)
			{
				const EVeyraClientState Candidate = static_cast<EVeyraClientState>(Index);
				for (const TPair<EVeyraClientIntent, EVeyraClientState>& Owner : Owners)
				{
					ASSERT_THAT(AreEqual(FVeyraClientFlow::IsIntentAllowed(Candidate, Owner.Key), Candidate == Owner.Value,
						FString::Printf(TEXT("%s in %s"), LexToString(Owner.Key), LexToString(Candidate))));
				}
				for (const EVeyraClientIntent Intent : Social)
				{
					const bool bPanel = Candidate == EVeyraClientState::Shell || Candidate == EVeyraClientState::Lobby;
					ASSERT_THAT(AreEqual(FVeyraClientFlow::IsIntentAllowed(Candidate, Intent), bPanel, FString::Printf(TEXT("%s in %s"), LexToString(Intent), LexToString(Candidate))));
				}
				ASSERT_THAT(IsTrue(FVeyraClientFlow::IsIntentAllowed(Candidate, EVeyraClientIntent::Retry)));
				ASSERT_THAT(IsTrue(FVeyraClientFlow::IsIntentAllowed(Candidate, EVeyraClientIntent::Quit)));
			}

			// A busy request holds every intent but Quit.
			ASSERT_THAT(IsTrue(ReachShell()));
			ASSERT_THAT(IsTrue(Flow->CanIssue(EVeyraClientIntent::StartPractice)));
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::Retry)));
			ASSERT_THAT(IsTrue(Flow->StartPractice()));
			ASSERT_THAT(IsTrue(Flow->GetSnapshot().bBusy));
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::StartPractice)));
			ASSERT_THAT(IsFalse(Flow->StartPractice()));
			ASSERT_THAT(IsTrue(Flow->CanIssue(EVeyraClientIntent::Quit)));
		}

		TEST_METHOD(SignInFailedStays)
		{
			TestRunner->AddExpectedMessagePlain(TEXT("VeyraClientFlow: signing in failed"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1);
			Flow->Start();
			ASSERT_THAT(AreEqual(Host.HandshakeLines.Num(), 1));
			ASSERT_THAT(AreEqual(Host.HandshakeLines[0], FString(VeyraLaunchHandshake::AwaitingLaunchCode)));
			Host.Input.Add(TEXT("not a launch code"));
			Flow->Tick();
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::SignInFailed));
			ASSERT_THAT(AreEqual(Host.HandshakeLines.Last(), VeyraLaunchHandshake::FailedLine(VeyraLaunchHandshake::EFailure::InvalidLaunchCode)));
			ASSERT_THAT(AreEqual(Flow->GetSnapshot().Problem->Code, FString(TEXT("invalid_launch_code"))));
			ASSERT_THAT(IsTrue(Backend.Pending.IsEmpty()));

			// The game waits for the player's choice: a launch code works once, so only Quit helps.
			Advance(3600.0);
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::SignInFailed));
			ASSERT_THAT(IsFalse(Host.bQuit));
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::Retry)));
			ASSERT_THAT(IsTrue(Flow->Quit()));
			ASSERT_THAT(IsTrue(Host.bQuit));
		}

		TEST_METHOD(ARefusedLaunchCodeFailsSignIn)
		{
			TestRunner->AddExpectedMessagePlain(TEXT("VeyraClientFlow: signing in failed"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1);
			Flow->Start();
			Host.Input.Add(ExampleCredential(TEXT("vlc_"), TEXT('A')));
			Flow->Tick();
			const FFlowTestBackend::FRequest* Redeem = Backend.Find(TEXT("POST"), TEXT("/v1/game-sessions"));
			ASSERT_THAT(IsNotNull(Redeem));
			ASSERT_THAT(IsTrue(Redeem->Credential.IsEmpty()));
			ASSERT_THAT(IsTrue(Redeem->Body.Contains(TEXT("\"buildVersion\":\"0.1.0\""))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), TEXT("/v1/game-sessions"), 401, ErrorBody(TEXT("invalid_credentials")))));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::SignInFailed));
			ASSERT_THAT(AreEqual(Host.HandshakeLines.Last(), VeyraLaunchHandshake::FailedLine(VeyraLaunchHandshake::EFailure::SignInRefused)));
			ASSERT_THAT(IsFalse(Host.bQuit));
		}

		TEST_METHOD(ReconnectOutranksTutorial)
		{
			ASSERT_THAT(IsTrue(SignIn()));
			ASSERT_THAT(AreEqual(Host.HandshakeLines.Last(), FString(VeyraLaunchHandshake::SignedIn)));
			ASSERT_THAT(AreEqual(Flow->GetSnapshot().DisplayName, FString(TEXT("DevOne"))));
			ASSERT_THAT(AreEqual(Backend.Find(TEXT("GET"), TEXT("/v1/me/match"))->Credential, GameSession()));

			// A live match comes first, even before a starter is chosen: nothing else is asked.
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/match"), 200, ReadyMatch())));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::ReconnectOnly));
			ASSERT_THAT(AreEqual(Flow->GetSnapshot().MatchId, FString(MatchId)));
			ASSERT_THAT(IsTrue(Backend.Pending.IsEmpty()));
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::StartPractice)));

			ASSERT_THAT(IsTrue(Flow->Reconnect()));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/match"), 200, ReadyMatch())));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::Connecting));
			ASSERT_THAT(AreEqual(Host.Travels.Last(), FString(TEXT("match ")) + Server));
			ASSERT_THAT(AreEqual(Host.JoinTicket, Ticket()));
		}

		TEST_METHOD(PracticeToMatch)
		{
			// A player without a starter chooses one first (ADR-010 §6).
			ASSERT_THAT(IsTrue(SignIn()));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/match"), 200, NoMatch)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/select"), 200, NoSelect)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/profile"), 200, ProfileBody(false))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/vanguards"), 200, VanguardsBody)));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::StarterChoice));
			ASSERT_THAT(AreEqual(Flow->GetSnapshot().Starters.Num(), 4));
			ASSERT_THAT(IsFalse(Flow->ChooseStarter(TEXT("test_vanguard"))));
			ASSERT_THAT(IsTrue(Flow->ChooseStarter(TEXT("oriel"))));
			ASSERT_THAT(AreEqual(Backend.Find(TEXT("POST"), TEXT("/v1/me/starter"))->Body, FString(TEXT("{\"vanguardId\":\"oriel\"}"))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), TEXT("/v1/me/starter"), 200, ProfileBody(true))));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::Shell));

			// Practice opens a select at once.
			ASSERT_THAT(IsTrue(Flow->StartPractice()));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), TEXT("/v1/practice"), 201, SelectBody(TEXT("picking")))));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::Selecting));
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::HoverVanguard)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/vanguards"), 200, VanguardsBody)));
			ASSERT_THAT(AreEqual(Flow->GetSnapshot().AvailableVanguards.Num(), 4));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Flow->GetRemainingPickSeconds(), 30.0)));

			// The pick timer is the backend's, read on every poll.
			Advance(0.5);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/select"), 200, SelectBody(TEXT("picking"), FString(), FString(), FString(), FString(), 29.0))));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Flow->GetRemainingPickSeconds(), 29.0)));

			ASSERT_THAT(IsTrue(Flow->HoverVanguard(TEXT("oriel"))));
			ASSERT_THAT(AreEqual(Backend.Find(TEXT("PUT"), TEXT("/v1/me/select/hover"))->Body, FString(TEXT("{\"vanguardId\":\"oriel\"}"))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("PUT"), TEXT("/v1/me/select/hover"), 200, SelectBody(TEXT("picking"), TEXT("oriel")))));
			ASSERT_THAT(AreEqual(Flow->GetSnapshot().Select.FindYou()->Hover, FString(TEXT("oriel"))));

			// Locking the only pick creates the match; its server starts, then the game joins it.
			Advance(0.5);
			ASSERT_THAT(IsNotNull(Backend.Find(TEXT("GET"), TEXT("/v1/me/select"))));
			ASSERT_THAT(IsTrue(Flow->LockVanguard(TEXT("oriel"))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), TEXT("/v1/me/select/lock"), 200, SelectBody(TEXT("started"), TEXT("oriel"), TEXT("oriel"), MatchId))));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::MatchStarting));
			ASSERT_THAT(AreEqual(Flow->GetSnapshot().MatchId, FString(MatchId)));
			// A read of the select sent before the lock answered is stale now, and changes nothing.
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/select"), 200, SelectBody(TEXT("picking")))));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::MatchStarting));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/match"), 200, StartingMatch())));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::MatchStarting));
			Advance(1.0);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/match"), 200, ReadyMatch())));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::Connecting));
			ASSERT_THAT(AreEqual(Host.Travels.Last(), FString(TEXT("match ")) + Server));
			ASSERT_THAT(AreEqual(Host.JoinTicket, Ticket()));

			Flow->NotifyWorld(EVeyraClientWorld::Match);
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::InMatch));
			ASSERT_THAT(IsFalse(Host.bQuit));
		}

		TEST_METHOD(FluxSpellsAreChosenFreelyAndMoveBetweenSlots)
		{
			ASSERT_THAT(IsTrue(ReachSelect()));
			const auto Spells = [this] { return Flow->GetSnapshot().Select.FindYou()->FluxSpells; };
			ASSERT_THAT(IsTrue(Flow->ChooseFluxSpell(0, TEXT("blink"))));
			ASSERT_THAT(AreEqual(Backend.Find(TEXT("PUT"), TEXT("/v1/me/select/spells"))->Body, FString(TEXT("{\"fluxSpells\":[\"blink\",\"\"]}"))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("PUT"), TEXT("/v1/me/select/spells"), 200,
				SelectBody(TEXT("picking"), FString(), FString(), FString(), FString(), 29.0, TEXT("[\"blink\",\"\"]")))));
			ASSERT_THAT(IsTrue(Spells() == TArray<FString>{ TEXT("blink"), FString() }));

			// The first slot's spell chosen for the second moves over. Here the backend refuses it.
			TestRunner->AddExpectedMessagePlain(TEXT("the backend refused the Flux Spells"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
			ASSERT_THAT(IsTrue(Flow->ChooseFluxSpell(1, TEXT("blink"))));
			ASSERT_THAT(AreEqual(Backend.Find(TEXT("PUT"), TEXT("/v1/me/select/spells"))->Body, FString(TEXT("{\"fluxSpells\":[\"\",\"blink\"]}"))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("PUT"), TEXT("/v1/me/select/spells"), 400, TEXT("{\"error\":\"invalid_flux_spells\"}"))));
			ASSERT_THAT(IsTrue(Flow->GetSnapshot().Problem.IsSet(), TEXT("a refusal is shown")));

			// Locked in, the spells stay open to change until the match starts (Pre-Game Client UX Bible 36).
			Advance(0.5);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/select"), 200,
				SelectBody(TEXT("picking"), TEXT("oriel"), TEXT("oriel"), FString(), FString(), 20.0, TEXT("[\"blink\",\"\"]")))));
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::HoverVanguard)));
			ASSERT_THAT(IsFalse(Flow->ChooseFluxSpell(2, TEXT("mend")), TEXT("there are two slots")));
			ASSERT_THAT(IsTrue(Flow->ChooseFluxSpell(1, TEXT("mend"))));
			ASSERT_THAT(AreEqual(Backend.Find(TEXT("PUT"), TEXT("/v1/me/select/spells"))->Body, FString(TEXT("{\"fluxSpells\":[\"blink\",\"mend\"]}"))));
		}

		TEST_METHOD(ACancelledSelectReturnsToTheShell)
		{
			ASSERT_THAT(IsTrue(ReachSelect()));
			Advance(0.5);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/select"), 200, NoSelect)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), SelectPath(), 200, SelectBody(TEXT("cancelled"), FString(), FString(), FString(), TEXT("timed_out"), 0.0))));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::Shell));
			ASSERT_THAT(AreEqual(Flow->GetSnapshot().Notice, FString(TEXT("timed_out"))));
			ASSERT_THAT(IsTrue(Flow->CanIssue(EVeyraClientIntent::StartPractice)));
		}

		TEST_METHOD(AServerThatQuitsWhileThePlayerWatchesTheEndLeavesItForTheResults)
		{
			ASSERT_THAT(IsTrue(ReachMatch()));
			Flow->NotifyMatchPhase(EVeyraMatchPhase::Ended);
			Flow->NotifyConnectionFailed(TEXT("ConnectionLost: the server closed the connection"));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::Returning));
			ASSERT_THAT(IsTrue(Flow->GetSnapshot().Notice.IsEmpty(), TEXT("an ended match, not a lost connection")));
			Flow->NotifyWorld(EVeyraClientWorld::FrontEnd);
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::AwaitingResults));
			Advance(FClientFlowTestRig::EndingShowSeconds);
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::AwaitingResults, TEXT("the wait it no longer needs does nothing")));
		}

		TEST_METHOD(MatchEndedToResults)
		{
			ASSERT_THAT(IsTrue(ReachMatch()));
			Flow->NotifyMatchPhase(EVeyraMatchPhase::Live);
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::InMatch));

			// The replicated end sends the game back to the front end to wait for the verified result, once
			// the player has watched the match end (ADR-020 §1).
			Flow->NotifyMatchPhase(EVeyraMatchPhase::Ended);
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::InMatch));
			Advance(FClientFlowTestRig::EndingShowSeconds - 1.0);
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::InMatch, TEXT("still watching")));
			Advance(1.0);
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::Returning));
			ASSERT_THAT(AreEqual(Host.Travels.Last(), FString(TEXT("front end"))));
			ASSERT_THAT(IsTrue(Host.JoinTicket.IsEmpty()));
			// The server closing the connection as it quits changes nothing.
			Flow->NotifyConnectionFailed(TEXT("ConnectionLost: the server closed the connection"));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::Returning));
			Flow->NotifyWorld(EVeyraClientWorld::FrontEnd);
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::AwaitingResults));

			// Until the server's report arrives, the backend still has the match ready.
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), MatchOutcomePath(), 200, OutcomeBody(TEXT("ready"), false))));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::AwaitingResults));
			Advance(1.0);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), MatchOutcomePath(), 200, OutcomeBody(TEXT("ended"), true))));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::Results));
			const TOptional<VeyraBackendProtocol::FMatchOutcome>& Result = Flow->GetSnapshot().Result;
			ASSERT_THAT(IsTrue(Result.IsSet() && Result->bHasResult));
			ASSERT_THAT(AreEqual(Result->EndReason, FString(TEXT("host_ended"))));
			ASSERT_THAT(IsTrue(Result->Winner.IsEmpty()));
			ASSERT_THAT(AreEqual(Result->VanguardId, FString(TEXT("oriel"))));
			ASSERT_THAT(IsTrue(Result->bJoined && Result->bConnectedAtEnd));

			// Continue finds where the player is, which is the shell.
			ASSERT_THAT(IsTrue(Flow->ContinueFromResults()));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::Loading));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/match"), 200, NoMatch)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/select"), 200, NoSelect)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/profile"), 200, ProfileBody(true))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/lobby"), 200, NoLobby)));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::Shell));
			ASSERT_THAT(IsFalse(Flow->GetSnapshot().Result.IsSet()));
		}

		TEST_METHOD(LeavingAMatchOnPurposeLeavesOnlyReconnectWhileItRuns)
		{
			// ADR-053 §1: Leave Match is a disconnect the player chose. The Vanguard plays on, and Reconnect returns to it.
			ASSERT_THAT(IsTrue(ReachMatch()));
			ASSERT_THAT(IsTrue(Flow->CanIssue(EVeyraClientIntent::LeaveMatch)));
			ASSERT_THAT(IsTrue(Flow->LeaveLiveMatch()));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::Returning));
			ASSERT_THAT(AreEqual(Flow->GetSnapshot().Notice, FString(TEXT("left_match"))));
			ASSERT_THAT(AreEqual(Host.Travels.Last(), FString(TEXT("front end"))));
			ASSERT_THAT(IsFalse(Flow->LeaveLiveMatch(), TEXT("only from a match")));
			Flow->NotifyWorld(EVeyraClientWorld::FrontEnd);
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::AwaitingResults));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), MatchOutcomePath(), 200, OutcomeBody(TEXT("ready"), false))));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::ReconnectOnly && Flow->CanIssue(EVeyraClientIntent::Reconnect)));
			ASSERT_THAT(AreEqual(Flow->GetSnapshot().Notice, FString(TEXT("left_match")), TEXT("Reconnect-only says why the player is there")));
		}

		TEST_METHOD(ThePlayStreakCountsUpToTheResultsAndADismissalStartsItAgain)
		{
			// ADR-053 §4: the reminder reads how long the player had played in a row when they left the match.
			constexpr double Played = 7300.0;
			ASSERT_THAT(IsTrue(ReachMatch()));
			Advance(Played);
			Flow->NotifyMatchPhase(EVeyraMatchPhase::Ended);
			Advance(FClientFlowTestRig::EndingShowSeconds);
			Flow->NotifyWorld(EVeyraClientWorld::FrontEnd);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), MatchOutcomePath(), 200, OutcomeBody(TEXT("ended"), true)) && State() == EVeyraClientState::Results));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Flow->GetSnapshot().PlayedSeconds, Played + FClientFlowTestRig::EndingShowSeconds),
				FString::SanitizeFloat(Flow->GetSnapshot().PlayedSeconds)));
			ASSERT_THAT(IsTrue(FVeyraClientFlow::IsIntentAllowed(EVeyraClientState::Results, EVeyraClientIntent::DismissPlayReminder)
				&& FVeyraClientFlow::IsIntentAllowed(EVeyraClientState::Shell, EVeyraClientIntent::DismissPlayReminder)
				&& !FVeyraClientFlow::IsIntentAllowed(EVeyraClientState::InMatch, EVeyraClientIntent::DismissPlayReminder), TEXT("after a match only")));
			ASSERT_THAT(IsTrue(Flow->DismissPlayReminder()));
			ASSERT_THAT(IsTrue(Flow->GetSnapshot().PlayedSeconds == 0.0, TEXT("dismissing counts again from now")));
		}

		TEST_METHOD(NetworkFailureToReconnect)
		{
			TestRunner->AddExpectedMessagePlain(TEXT("VeyraClientFlow: the connection to match"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
			ASSERT_THAT(IsTrue(ReachMatch()));
			Flow->NotifyConnectionFailed(TEXT("ConnectionTimeout: no answer"));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::Returning));
			ASSERT_THAT(AreEqual(Flow->GetSnapshot().Notice, FString(TEXT("connection_lost"))));
			ASSERT_THAT(AreEqual(Host.Travels.Last(), FString(TEXT("front end"))));
			Flow->NotifyWorld(EVeyraClientWorld::FrontEnd);
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::AwaitingResults));

			// The match runs without the player: Reconnect is all they may do (UX-17).
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), MatchOutcomePath(), 200, OutcomeBody(TEXT("ready"), false))));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::ReconnectOnly));
			ASSERT_THAT(AreEqual(Flow->GetSnapshot().Notice, FString(TEXT("connection_lost"))));
			ASSERT_THAT(IsTrue(Flow->CanIssue(EVeyraClientIntent::Reconnect)));

			// Once it ends without them, its result shows.
			Advance(5.0);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/match"), 200, NoMatch)));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::AwaitingResults));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), MatchOutcomePath(), 200, OutcomeBody(TEXT("ended"), true))));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::Results));
			ASSERT_THAT(IsFalse(Host.bQuit));
		}

		TEST_METHOD(SessionEnded)
		{
			TestRunner->AddExpectedMessagePlain(TEXT("VeyraClientFlow: the backend ended the game session"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
			ASSERT_THAT(IsTrue(ReachShell()));
			ASSERT_THAT(IsTrue(Flow->StartPractice()));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), TEXT("/v1/practice"), 401, ErrorBody(TEXT("invalid_credentials")))));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::SessionEnded));
			ASSERT_THAT(AreEqual(Flow->GetSnapshot().Problem->Code, FString(TEXT("session_ended"))));
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::StartPractice)));
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::Retry)));
			ASSERT_THAT(IsFalse(Host.bQuit));
		}

		TEST_METHOD(AnUnansweredRequestIsTriedAgainThenOffersRetry)
		{
			TestRunner->AddExpectedMessagePlain(TEXT("VeyraClientFlow: problem in Loading (backend_unreachable)"), ELogVerbosity::Warning,
				EAutomationExpectedMessageFlags::Contains, 1);
			ASSERT_THAT(IsTrue(SignIn()));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/match"), 0)));
			ASSERT_THAT(IsNull(Backend.Find(TEXT("GET"), TEXT("/v1/me/match"))));
			Advance(1.0);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/match"), 503)));
			ASSERT_THAT(AreEqual(Flow->GetSnapshot().Problem->Code, FString(TEXT("backend_unreachable"))));
			ASSERT_THAT(IsTrue(Flow->CanIssue(EVeyraClientIntent::Retry)));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::Loading));

			ASSERT_THAT(IsTrue(Flow->Retry()));
			ASSERT_THAT(IsFalse(Flow->GetSnapshot().Problem.IsSet()));
			ASSERT_THAT(IsTrue(ResumeToShell()));
		}

		TEST_METHOD(QueueCancel)
		{
			ASSERT_THAT(IsTrue(ReachShell()));
			// The shell reads the modes and the party; until the modes arrive, none can be chosen.
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::SelectMode)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/modes"), 200, ModesBody)));
			ASSERT_THAT(AreEqual(Flow->GetSnapshot().Modes.Num(), 2));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/party"), 200, NoParty)));
			ASSERT_THAT(IsFalse(Flow->GetSnapshot().Party.IsSet()));

			// A mode without a matchmaker is not on offer; without a party nothing can be readied or queued.
			ASSERT_THAT(IsTrue(Flow->CanIssue(EVeyraClientIntent::SelectMode)));
			ASSERT_THAT(IsFalse(Flow->SelectMode(UnmatchedMode)));
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::SetReady)));
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::FindMatch)));
			ASSERT_THAT(IsTrue(Flow->SelectMode(CasualMode)));
			ASSERT_THAT(AreEqual(Backend.Find(TEXT("PUT"), TEXT("/v1/party/mode"))->Body, FString(TEXT("{\"mode\":\"casual_select\"}"))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("PUT"), TEXT("/v1/party/mode"), 200, PartyBody(TEXT("idle"), false))));
			// Find Match waits until everyone is Ready (UX-6).
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::FindMatch)));

			// A read of the party sent before Ready and answered after it is stale, and changes nothing.
			Advance(1.0);
			ASSERT_THAT(IsTrue(Flow->SetReady(true)));
			ASSERT_THAT(AreEqual(Backend.Find(TEXT("PUT"), TEXT("/v1/party/ready"))->Body, FString(TEXT("{\"ready\":true}"))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("PUT"), TEXT("/v1/party/ready"), 200, PartyBody(TEXT("idle"), true))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/party"), 200, PartyBody(TEXT("idle"), false))));
			ASSERT_THAT(IsTrue(Flow->GetSnapshot().Party->AllReady()));
			ASSERT_THAT(IsTrue(Flow->CanIssue(EVeyraClientIntent::FindMatch)));

			ASSERT_THAT(IsTrue(Flow->FindMatch()));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), TEXT("/v1/party/queue"), 200, PartyBody(TEXT("queued"), true))));
			ASSERT_THAT(IsTrue(Flow->GetSnapshot().Party->Status == VeyraBackendProtocol::EPartyStatus::Queued));
			// The queue time is the backend's, counted on between reads (UX-2).
			Advance(1.0);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/party"), 200, PartyBody(TEXT("queued"), true, 7.0))));
			Advance(0.5);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Flow->GetQueuedSeconds(), 7.5)));

			// A queued party cannot practise, change its mode or unready; its leader's Cancel takes it out.
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::StartPractice)));
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::SelectMode)));
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::SetReady)));
			ASSERT_THAT(IsTrue(Flow->CancelQueue()));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("DELETE"), TEXT("/v1/party/queue"), 200, PartyBody(TEXT("idle"), false))));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::Shell));
			ASSERT_THAT(IsTrue(Flow->GetQueuedSeconds() == 0.0));
			ASSERT_THAT(IsTrue(Flow->CanIssue(EVeyraClientIntent::StartPractice)));
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::FindMatch)));
		}

		TEST_METHOD(MatchFoundAcceptedOpensTheSelect)
		{
			ASSERT_THAT(IsTrue(ReachMatchFound()));
			// A match found holds everything else until it is answered (Parties & Social Bible §3).
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::CancelQueue)));
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::StartPractice)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Flow->GetRemainingAcceptSeconds(), 15.0)));
			ASSERT_THAT(IsTrue(Flow->AcceptMatch()));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), TEXT("/v1/me/match-found/accept"), 200,
				MatchFoundBody(TEXT("pending"), TEXT("accepted"), 1, FString(), FString(), 14.0))));

			// The answer is given once; the player waits for the others.
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::MatchFound));
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::AcceptMatch)));
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::DeclineMatch)));
			ASSERT_THAT(AreEqual(Flow->GetSnapshot().MatchFound.Accepted, 1));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Flow->GetRemainingAcceptSeconds(), 14.0)));

			// The last acceptance opens the select. Only a match found that waits is reported, so the next read is empty.
			Advance(0.5);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/match-found"), 200, NoMatchFound)));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::Loading));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/match"), 200, NoMatch)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/select"), 200, CasualSelectBody(TEXT("picking")))));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::Selecting));
			ASSERT_THAT(IsTrue(Flow->GetSnapshot().Notice.IsEmpty()));
			ASSERT_THAT(AreEqual(Flow->GetSnapshot().Select.Seats.Num(), 2));
			ASSERT_THAT(IsTrue(Flow->CanIssue(EVeyraClientIntent::LeaveSelect)));
		}

		TEST_METHOD(MatchFoundDeclinedLeavesTheQueue)
		{
			ASSERT_THAT(IsTrue(ReachMatchFound()));
			ASSERT_THAT(IsTrue(Flow->DeclineMatch()));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), TEXT("/v1/me/match-found/decline"), 200,
				MatchFoundBody(TEXT("abandoned"), TEXT("declined"), 0, FString(), TEXT("declined"), 0.0))));
			ASSERT_THAT(IsTrue(ResumeToShell()));
			ASSERT_THAT(AreEqual(Flow->GetSnapshot().Notice, FString(TEXT("match_found_declined"))));

			// The decliner's party left the queue, Not Ready (§3).
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/party"), 200, PartyBody(TEXT("idle"), false))));
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::CancelQueue)));
			ASSERT_THAT(IsTrue(Flow->CanIssue(EVeyraClientIntent::SetReady)));
		}

		TEST_METHOD(MatchFoundAbandonedByAnotherRequeues)
		{
			ASSERT_THAT(IsTrue(ReachMatchFound()));
			ASSERT_THAT(IsTrue(Flow->AcceptMatch()));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), TEXT("/v1/me/match-found/accept"), 200, MatchFoundBody(TEXT("pending"), TEXT("accepted"), 1))));

			// The other player declines. Nobody learns who; the party read says the player who accepted is
			// queued again, keeping their place.
			Advance(0.5);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/match-found"), 200, NoMatchFound)));
			ASSERT_THAT(IsTrue(ResumeToShell()));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/party"), 200, PartyBody(TEXT("queued"), true, 12.0))));
			ASSERT_THAT(AreEqual(Flow->GetSnapshot().Notice, FString(TEXT("match_found_requeued"))));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Flow->GetQueuedSeconds(), 12.0)));
			ASSERT_THAT(IsTrue(Flow->CanIssue(EVeyraClientIntent::CancelQueue)));

			// Only that first read explains it: queueing again later changes no notice.
			ASSERT_THAT(IsTrue(Flow->CancelQueue()));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("DELETE"), TEXT("/v1/party/queue"), 200, PartyBody(TEXT("idle"), false))));
			ASSERT_THAT(AreEqual(Flow->GetSnapshot().Notice, FString(TEXT("match_found_requeued"))));
		}

		TEST_METHOD(MatchFoundMissedLeavesTheQueue)
		{
			ASSERT_THAT(IsTrue(ReachMatchFound()));
			// The timer runs out without an answer.
			Advance(0.5);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/match-found"), 200, MatchFoundBody(TEXT("pending"), TEXT("pending"), 1, FString(), FString(), 0.5))));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Flow->GetRemainingAcceptSeconds(), 0.5)));
			Advance(0.5);
			ASSERT_THAT(IsTrue(Flow->GetRemainingAcceptSeconds() == 0.0));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/match-found"), 200, NoMatchFound)));
			ASSERT_THAT(IsTrue(ResumeToShell()));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/party"), 200, PartyBody(TEXT("idle"), false))));
			ASSERT_THAT(AreEqual(Flow->GetSnapshot().Notice, FString(TEXT("match_found_missed"))));
		}

		TEST_METHOD(MatchFoundDeclinedByAnotherBeforeAnAnswerRequeues)
		{
			// Another player declines before this one answers: not at fault, so queued again.
			ASSERT_THAT(IsTrue(ReachMatchFound()));
			Advance(0.5);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/match-found"), 200, NoMatchFound)));
			ASSERT_THAT(IsTrue(ResumeToShell()));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/party"), 200, PartyBody(TEXT("queued"), true, 4.0))));
			ASSERT_THAT(AreEqual(Flow->GetSnapshot().Notice, FString(TEXT("match_found_requeued"))));
		}

		TEST_METHOD(LeavingACasualSelectDodges)
		{
			ASSERT_THAT(IsTrue(Rig.ReachCasualSelect()));
			ASSERT_THAT(IsTrue(Flow->LeaveSelect()));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), TEXT("/v1/me/select/leave"), 200, CasualSelectBody(TEXT("cancelled"), TEXT("left")))));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::Shell));
			ASSERT_THAT(AreEqual(Flow->GetSnapshot().Notice, FString(TEXT("you_left"))));
			// The leaver's party left the queue, Not Ready (Match Flow Bible §2).
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/party"), 200, PartyBody(TEXT("idle"), false))));
			ASSERT_THAT(IsTrue(Flow->CanIssue(EVeyraClientIntent::StartPractice)));
		}

		TEST_METHOD(ADraftBansAndPicksInTurn)
		{
			// The player's ban turn: they ban any released Vanguard, owned or not, and cannot lock a pick.
			const TCHAR* const ABan = TEXT("{\"ban\":true,\"side\":\"A\",\"count\":1,\"done\":0}");
			const TCHAR* const Idle = TEXT("\"hover\":null,\"locked\":null,\"acting\":false");
			ASSERT_THAT(IsTrue(Rig.ReachDraftSelect(DraftSelectBody(TEXT("banning"), ABan, TEXT("[]"), TEXT("\"hover\":\"oriel\",\"locked\":null,\"acting\":true"), Idle))));
			ASSERT_THAT(IsTrue(Flow->CanIssue(EVeyraClientIntent::LeaveSelect), TEXT("a draft may be left, as a dodge")));
			ASSERT_THAT(IsTrue(Flow->CanIssue(EVeyraClientIntent::BanVanguard) && Flow->CanIssue(EVeyraClientIntent::HoverVanguard)));
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::LockVanguard), TEXT("no pick in a ban turn")));
			ASSERT_THAT(IsFalse(Flow->BanVanguard(TEXT("nobody")), TEXT("only a released Vanguard")));
			ASSERT_THAT(IsTrue(Flow->HoverBan(TEXT("silt"))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("PUT"), TEXT("/v1/me/select/ban/hover"), 200,
				DraftSelectBody(TEXT("banning"), ABan, TEXT("[]"), TEXT("\"hover\":\"oriel\",\"locked\":null,\"banHover\":\"silt\",\"acting\":true"), Idle))));
			ASSERT_THAT(AreEqual(Flow->GetSnapshot().Select.FindYou()->BanHover, FString(TEXT("silt"))));
			ASSERT_THAT(IsTrue(Flow->BanVanguard(TEXT("silt"))));
			const auto* Ban = Backend.Find(TEXT("POST"), TEXT("/v1/me/select/ban"));
			ASSERT_THAT(IsTrue(Ban && Ban->Body.Contains(TEXT("\"vanguardId\":\"silt\""))));

			// Side B's ban turn: nobody on side A bans, and a banned Vanguard cannot be banned again.
			const TCHAR* const BBan = TEXT("{\"ban\":true,\"side\":\"B\",\"count\":1,\"done\":0}");
			const TCHAR* const Silt = TEXT("[{\"side\":\"A\",\"vanguardId\":\"silt\"}]");
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), TEXT("/v1/me/select/ban"), 200,
				DraftSelectBody(TEXT("banning"), BBan, Silt, TEXT("\"hover\":\"oriel\",\"locked\":null,\"acting\":false"), Idle))));
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::BanVanguard)));
			ASSERT_THAT(IsTrue(Flow->GetSnapshot().Select.IsBanned(TEXT("silt"))));

			// The team's pick turn names the player: they lock their hover; their teammate waits.
			const TCHAR* const APick = TEXT("{\"ban\":false,\"side\":\"A\",\"count\":1,\"done\":0}");
			Advance(0.5);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/select"), 200,
				DraftSelectBody(TEXT("picking"), APick, Silt, TEXT("\"hover\":\"oriel\",\"locked\":null,\"acting\":true"), Idle))));
			ASSERT_THAT(IsTrue(Flow->CanIssue(EVeyraClientIntent::LockVanguard) && !Flow->CanIssue(EVeyraClientIntent::BanVanguard)));
			ASSERT_THAT(IsTrue(Flow->LockVanguard(TEXT("oriel"))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), TEXT("/v1/me/select/lock"), 200,
				DraftSelectBody(TEXT("picking"), TEXT("{\"ban\":false,\"side\":\"B\",\"count\":1,\"done\":0}"), Silt,
					TEXT("\"hover\":\"oriel\",\"locked\":\"oriel\",\"acting\":false"), TEXT("\"hover\":null,\"locked\":null,\"acting\":false")))));
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::LockVanguard), TEXT("a lock is permanent")));
		}

		TEST_METHOD(LockedTeammatesTrade)
		{
			// The final window: the player and DevThree are locked; DevTwo is on the other team.
			const TCHAR* const Locked = TEXT("\"hover\":\"oriel\",\"locked\":\"oriel\",\"acting\":false");
			const TCHAR* const TeammateLocked = TEXT("\"hover\":\"cairn\",\"locked\":\"cairn\",\"acting\":false");
			ASSERT_THAT(IsTrue(Rig.ReachDraftSelect(DraftSelectBody(TEXT("final"), TEXT("null"), TEXT("[]"), Locked, TeammateLocked))));
			ASSERT_THAT(IsTrue(Flow->CanIssue(EVeyraClientIntent::OfferTrade) && !Flow->CanIssue(EVeyraClientIntent::AnswerTrade)));
			ASSERT_THAT(IsFalse(Flow->OfferTrade(0), TEXT("not with themselves")));
			ASSERT_THAT(IsFalse(Flow->OfferTrade(2), TEXT("not with an enemy")));
			ASSERT_THAT(IsTrue(Flow->OfferTrade(1)));
			const auto* Offer = Backend.Find(TEXT("POST"), TEXT("/v1/me/select/trade"));
			ASSERT_THAT(IsTrue(Offer && Offer->Body == TEXT("{\"seat\":1}")));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), TEXT("/v1/me/select/trade"), 200, DraftSelectBody(TEXT("final"), TEXT("null"), TEXT("[]"),
				TEXT("\"hover\":\"oriel\",\"locked\":\"oriel\",\"acting\":false,\"offeredByYou\":false"),
				TEXT("\"hover\":\"cairn\",\"locked\":\"cairn\",\"acting\":false,\"offeredByYou\":true")))));

			// DevThree offers one back, which the player accepts.
			Advance(0.5);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/select"), 200, DraftSelectBody(TEXT("final"), TEXT("null"), TEXT("[]"), Locked,
				TEXT("\"hover\":\"cairn\",\"locked\":\"cairn\",\"acting\":false,\"offersYou\":true")))));
			ASSERT_THAT(IsTrue(Flow->CanIssue(EVeyraClientIntent::AnswerTrade)));
			ASSERT_THAT(IsFalse(Flow->AnswerTrade(2, true), TEXT("only a standing offer")));
			ASSERT_THAT(IsTrue(Flow->AnswerTrade(1, true)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), TEXT("/v1/me/select/trade/accept"), 200, DraftSelectBody(TEXT("final"), TEXT("null"), TEXT("[]"),
				TEXT("\"hover\":\"cairn\",\"locked\":\"cairn\",\"acting\":false"), TEXT("\"hover\":\"oriel\",\"locked\":\"oriel\",\"acting\":false")))));
			ASSERT_THAT(AreEqual(Flow->GetSnapshot().Select.FindYou()->Locked, FString(TEXT("cairn"))));

			// A trade the backend refuses as illegal shows why.
			TestRunner->AddExpectedMessagePlain(TEXT("VeyraClientFlow: problem in Selecting"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
			Advance(0.5);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/select"), 200, DraftSelectBody(TEXT("final"), TEXT("null"), TEXT("[]"),
				TEXT("\"hover\":\"cairn\",\"locked\":\"cairn\",\"acting\":false"),
				TEXT("\"hover\":\"oriel\",\"locked\":\"oriel\",\"acting\":false,\"offersYou\":true")))));
			ASSERT_THAT(IsTrue(Flow->AnswerTrade(1, true)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), TEXT("/v1/me/select/trade/accept"), 400, TEXT("{\"error\":\"not_available\"}"))));
			ASSERT_THAT(IsTrue(Flow->GetSnapshot().Problem.IsSet()));
		}

		TEST_METHOD(AnOpponentLeavingRequeues)
		{
			ASSERT_THAT(IsTrue(Rig.ReachCasualSelect()));
			Advance(0.5);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/select"), 200, NoSelect)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), SelectPath(), 200, CasualSelectBody(TEXT("cancelled"), TEXT("left")))));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::Shell));
			ASSERT_THAT(AreEqual(Flow->GetSnapshot().Notice, FString(TEXT("left"))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/party"), 200, PartyBody(TEXT("queued"), true, 20.0))));
			ASSERT_THAT(IsTrue(Flow->CanIssue(EVeyraClientIntent::CancelQueue)));
		}

		TEST_METHOD(MatchHistoryLoadsMoreFiltersAndOpensARecord)
		{
			ASSERT_THAT(IsTrue(Rig.ReachShell()));
			ASSERT_THAT(IsTrue(Rig.Flow->LoadHistory({})));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/me/matches"), 200, HistoryBody({ HistoryEntry(MatchId, TEXT("win")) }, TEXT("\"cursor_1\"")))));
			const FVeyraMatchHistory& History = Rig.Flow->GetSnapshot().History;
			ASSERT_THAT(IsTrue(History.bLoaded && History.Entries.Num() == 1 && History.Entries[0].Outcome == TEXT("win") && History.Next == TEXT("cursor_1")));
			ASSERT_THAT(IsTrue(FMath::Abs((History.Entries[0].EndedAt - FDateTime(2026, 9, 29, 10, 3, 12, 123)).GetTotalMilliseconds()) <= 1.0 && History.Entries[0].DurationSeconds == 1510.5));

			// Older records come in batches, after those read (UX-67).
			ASSERT_THAT(IsTrue(Rig.Flow->CanIssue(EVeyraClientIntent::LoadMoreHistory) && Rig.Flow->LoadMoreHistory()));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/me/matches?cursor=cursor_1"), 200, HistoryBody({ HistoryEntry(OlderMatchId, TEXT("loss")) }, TEXT("null")))));
			ASSERT_THAT(IsTrue(History.Entries.Num() == 2 && History.Entries[1].MatchId == OlderMatchId && History.Next.IsEmpty()));
			ASSERT_THAT(IsFalse(Rig.Flow->CanIssue(EVeyraClientIntent::LoadMoreHistory), TEXT("the last page")));

			// A filter reads the first page afresh (UX-64).
			VeyraBackendProtocol::FHistoryFilter Losses;
			Losses.Outcome = TEXT("loss");
			Losses.VanguardId = TEXT("cairn");
			ASSERT_THAT(IsTrue(Rig.Flow->LoadHistory(Losses)));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/me/matches?vanguard=cairn&outcome=loss"), 200, HistoryBody({ HistoryEntry(OlderMatchId, TEXT("loss")) }, TEXT("null")))));
			ASSERT_THAT(IsTrue(History.Filter == Losses && History.Entries.Num() == 1));

			// A listed match opens into its verified result; nothing else does.
			ASSERT_THAT(IsFalse(Rig.Flow->OpenHistoryMatch(MatchId), TEXT("not listed under this filter")));
			ASSERT_THAT(IsTrue(Rig.Flow->OpenHistoryMatch(OlderMatchId)));
			ASSERT_THAT(IsTrue(Rig.Backend.Answer(TEXT("GET"), FString(TEXT("/v1/me/matches/")) + OlderMatchId, 200, ScoredOutcomeBody())));
			ASSERT_THAT(IsTrue(History.Opened.IsSet() && History.Opened->bHasScoreboard && !Rig.Flow->CanIssue(EVeyraClientIntent::LoadMoreHistory)));
			ASSERT_THAT(IsTrue(Rig.Flow->CloseHistoryMatch() && !History.Opened.IsSet()));

			// Not through Match Found, a committed select or Reconnect-only (UX-51).
			ASSERT_THAT(IsFalse(FVeyraClientFlow::IsIntentAllowed(EVeyraClientState::Selecting, EVeyraClientIntent::LoadHistory)));
			ASSERT_THAT(IsFalse(FVeyraClientFlow::IsIntentAllowed(EVeyraClientState::MatchFound, EVeyraClientIntent::OpenHistoryMatch)));
			ASSERT_THAT(IsFalse(FVeyraClientFlow::IsIntentAllowed(EVeyraClientState::ReconnectOnly, EVeyraClientIntent::LoadHistory)));
		}

		TEST_METHOD(APracticeSelectCannotBeLeft)
		{
			ASSERT_THAT(IsTrue(ReachSelect()));
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::LeaveSelect)));
			ASSERT_THAT(IsFalse(Flow->LeaveSelect()));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
