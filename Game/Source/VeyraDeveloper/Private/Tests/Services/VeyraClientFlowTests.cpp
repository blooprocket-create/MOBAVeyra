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

		TEST_METHOD(Transitions)
		{
			// Each intent belongs to one state; Retry and Quit are open to every state, subject to the snapshot.
			const TPair<EVeyraClientIntent, EVeyraClientState> Owners[] = {
				{ EVeyraClientIntent::ChooseStarter, EVeyraClientState::StarterChoice },
				{ EVeyraClientIntent::StartPractice, EVeyraClientState::Shell },
				{ EVeyraClientIntent::HoverVanguard, EVeyraClientState::Selecting },
				{ EVeyraClientIntent::LockVanguard, EVeyraClientState::Selecting },
				{ EVeyraClientIntent::Reconnect, EVeyraClientState::ReconnectOnly },
				{ EVeyraClientIntent::ContinueFromResults, EVeyraClientState::Results },
			};
			for (uint8 Index = 0; Index <= static_cast<uint8>(EVeyraClientState::SessionEnded); ++Index)
			{
				const EVeyraClientState Candidate = static_cast<EVeyraClientState>(Index);
				for (const TPair<EVeyraClientIntent, EVeyraClientState>& Owner : Owners)
				{
					ASSERT_THAT(AreEqual(FVeyraClientFlow::IsIntentAllowed(Candidate, Owner.Key), Candidate == Owner.Value,
						FString::Printf(TEXT("%s in %s"), LexToString(Owner.Key), LexToString(Candidate))));
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

		TEST_METHOD(MatchEndedToResults)
		{
			ASSERT_THAT(IsTrue(ReachMatch()));
			Flow->NotifyMatchPhase(EVeyraMatchPhase::Live);
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::InMatch));

			// The replicated end sends the game back to the front end to wait for the verified result.
			Flow->NotifyMatchPhase(EVeyraMatchPhase::Ended);
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
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::Shell));
			ASSERT_THAT(IsFalse(Flow->GetSnapshot().Result.IsSet()));
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
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/match"), 200, NoMatch)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/select"), 200, NoSelect)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/profile"), 200, ProfileBody(true))));
			ASSERT_THAT(IsTrue(State() == EVeyraClientState::Shell));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
