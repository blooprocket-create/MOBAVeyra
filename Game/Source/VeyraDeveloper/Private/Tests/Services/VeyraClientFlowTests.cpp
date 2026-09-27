// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER

#include "Backend/VeyraBackendTransport.h"
#include "Client/VeyraClientFlow.h"

namespace VeyraClientFlowTests
{
	const TCHAR* const AccountId = TEXT("11111111-2222-4333-8444-555555555555");
	const TCHAR* const SelectId = TEXT("22222222-3333-4444-8555-666666666666");
	const TCHAR* const MatchId = TEXT("33333333-4444-4555-8666-777777777777");
	const TCHAR* const Server = TEXT("127.0.0.1:7780");

	/** A value in a credential's format: its prefix and 43 base64url characters. It is not a credential. */
	FString ExampleCredential(const TCHAR* Prefix, TCHAR Fill)
	{
		return FString(Prefix) + FString::ChrN(43, Fill);
	}

	FString GameSession() { return ExampleCredential(TEXT("vgs_"), TEXT('B')); }
	FString Ticket() { return ExampleCredential(TEXT("vjt_"), TEXT('C')); }

	FString Quoted(const FString& Text)
	{
		return Text.IsEmpty() ? FString(TEXT("null")) : TEXT("\"") + Text + TEXT("\"");
	}

	FString ErrorBody(const TCHAR* Code)
	{
		return FString::Printf(TEXT("{\"error\":\"%s\"}"), Code);
	}

	FString SessionBody()
	{
		return FString::Printf(TEXT("{\"token\":\"%s\",\"expiresAt\":\"2026-09-27T12:00:00Z\",\"account\":{\"id\":\"%s\",\"displayName\":\"DevOne\"}}"), *GameSession(), AccountId);
	}

	const TCHAR* const NoMatch = TEXT("{\"match\":null}");

	FString StartingMatch()
	{
		return FString::Printf(TEXT("{\"match\":{\"id\":\"%s\",\"mode\":\"custom_practice\",\"rules\":\"practice\",\"state\":\"allocating\",\"side\":\"A\",")
							   TEXT("\"vanguardId\":\"oriel\",\"server\":null,\"ticket\":null}}"),
			MatchId);
	}

	FString ReadyMatch()
	{
		return FString::Printf(TEXT("{\"match\":{\"id\":\"%s\",\"mode\":\"custom_practice\",\"rules\":\"practice\",\"state\":\"ready\",\"side\":\"A\",")
							   TEXT("\"vanguardId\":\"oriel\",\"server\":{\"host\":\"127.0.0.1\",\"port\":7780},\"ticket\":\"%s\"}}"),
			MatchId, *Ticket());
	}

	const TCHAR* const NoSelect = TEXT("{\"select\":null}");

	FString SelectBody(const TCHAR* State, const FString& Hover = FString(), const FString& Locked = FString(), const FString& StartedMatch = FString(),
		const FString& CancelReason = FString(), double Remaining = 30.0)
	{
		return FString::Printf(TEXT("{\"select\":{\"id\":\"%s\",\"kind\":\"practice\",\"mode\":\"custom_practice\",\"state\":\"%s\",")
							   TEXT("\"deadline\":\"2026-09-27T12:00:30Z\",\"remainingSeconds\":%g,")
							   TEXT("\"seats\":[{\"displayName\":\"DevOne\",\"side\":\"A\",\"you\":true,\"hover\":%s,\"locked\":%s}],")
							   TEXT("\"matchId\":%s,\"cancelReason\":%s}}"),
			SelectId, State, Remaining, *Quoted(Hover), *Quoted(Locked), *Quoted(StartedMatch), *Quoted(CancelReason));
	}

	FString ProfileBody(bool bCompleted)
	{
		return FString::Printf(TEXT("{\"account\":{\"id\":\"%s\",\"displayName\":\"DevOne\"},\"tutorial\":{\"completed\":%s,\"starterVanguardId\":%s}}"), AccountId,
			bCompleted ? TEXT("true") : TEXT("false"), bCompleted ? TEXT("\"oriel\"") : TEXT("null"));
	}

	const TCHAR* const VanguardsBody = TEXT("{\"owned\":[],\"rotation\":[\"cairn\",\"qazharr\",\"oriel\",\"bryn\"],")
									   TEXT("\"available\":[\"cairn\",\"qazharr\",\"oriel\",\"bryn\"],\"starters\":[\"cairn\",\"qazharr\",\"oriel\",\"bryn\"]}");

	FString OutcomeBody(const TCHAR* State, bool bWithResult)
	{
		const FString Result = bWithResult
			? FString(TEXT("{\"endReason\":\"host_ended\",\"winner\":null,\"durationSeconds\":42.5,\"joined\":true,\"connectedAtEnd\":true}"))
			: FString(TEXT("null"));
		return FString::Printf(TEXT("{\"match\":{\"id\":\"%s\",\"mode\":\"custom_practice\",\"rules\":\"practice\",\"state\":\"%s\",\"side\":\"A\",")
							   TEXT("\"vanguardId\":\"oriel\",\"failureReason\":null,\"result\":%s}}"),
			MatchId, State, *Result);
	}

	FString MatchOutcomePath() { return FString(TEXT("/v1/me/matches/")) + MatchId; }
	FString SelectPath() { return FString(TEXT("/v1/me/selects/")) + SelectId; }

	/** A backend that holds every request until the test answers it. */
	class FFlowTestBackend final : public IVeyraBackendTransport
	{
	public:
		struct FRequest
		{
			FString Verb;
			FString Path;
			FString Credential;
			FString Body;
			FVeyraBackendCallback OnDone;
		};

		TArray<FRequest> Pending;

		virtual void Get(const FString& Path, const FString& Credential, FVeyraBackendCallback OnDone) override
		{
			Pending.Add({ TEXT("GET"), Path, Credential, FString(), MoveTemp(OnDone) });
		}

		virtual void Post(const FString& Path, const FString& Credential, const FString& Body, FVeyraBackendCallback OnDone) override
		{
			Pending.Add({ TEXT("POST"), Path, Credential, Body, MoveTemp(OnDone) });
		}

		virtual void Put(const FString& Path, const FString& Credential, const FString& Body, FVeyraBackendCallback OnDone) override
		{
			Pending.Add({ TEXT("PUT"), Path, Credential, Body, MoveTemp(OnDone) });
		}

		const FRequest* Find(const TCHAR* Verb, const FString& Path) const
		{
			return Pending.FindByPredicate([Verb, &Path](const FRequest& Request) { return Request.Verb == Verb && Request.Path == Path; });
		}

		/** Answers the oldest pending request with this verb and path; a status of 0 is no answer at all. False if none is pending. */
		bool Answer(const TCHAR* Verb, const FString& Path, int32 Status, const FString& Body = FString())
		{
			const int32 Index = Pending.IndexOfByPredicate([Verb, &Path](const FRequest& Request) { return Request.Verb == Verb && Request.Path == Path; });
			if (Index == INDEX_NONE)
			{
				return false;
			}
			FRequest Request = MoveTemp(Pending[Index]);
			Pending.RemoveAt(Index);
			FVeyraBackendResponse Response;
			Response.bAnswered = Status > 0;
			Response.Status = Status;
			Response.Body = Body;
			Request.OnDone(Response);
			return true;
		}
	};

	/** An engine that records what the flow asks of it. */
	class FFlowTestHost final : public IVeyraClientFlowHost
	{
	public:
		double Clock = 100.0;
		TArray<FString> Input;
		bool bInputClosed = false;
		TArray<FString> HandshakeLines;
		/** "match <address>" or "front end". */
		TArray<FString> Travels;
		FString JoinTicket;
		bool bQuit = false;

		virtual double Now() const override { return Clock; }

		virtual EVeyraPipeRead PollLaunchCode(FString& OutLine) override
		{
			if (!Input.IsEmpty())
			{
				OutLine = Input[0];
				Input.RemoveAt(0);
				return EVeyraPipeRead::Line;
			}
			return bInputClosed ? EVeyraPipeRead::EndOfInput : EVeyraPipeRead::Pending;
		}

		virtual void WriteHandshake(const FString& Line) override { HandshakeLines.Add(Line); }

		virtual bool TravelToMatch(const FString& Address, const FString& InTicket) override
		{
			Travels.Add(TEXT("match ") + Address);
			JoinTicket = InTicket;
			return true;
		}

		virtual void TravelToFrontEnd() override
		{
			Travels.Add(TEXT("front end"));
			JoinTicket.Reset();
		}

		virtual void QuitGame() override { bQuit = true; }
	};

	// Veyra.Services.ClientFlow.*: the client-state coordinator (ADR-010 §2), driven through a fake
	// backend and engine exactly as the game's presentation and the backend drive it.
	TEST_CLASS(ClientFlow, "Veyra.Services")
	{
		FFlowTestBackend Backend;
		FFlowTestHost Host;
		TUniquePtr<FVeyraClientFlow> Flow;

		BEFORE_EACH()
		{
			FVeyraClientFlowConfig Config;
			Config.BuildVersion = TEXT("0.1.0");
			Config.LaunchCodeReadTimeoutSeconds = 30.0;
			Config.RequestAttempts = 2;
			Config.RetryIntervalSeconds = 1.0;
			Config.SelectPollIntervalSeconds = 0.5;
			Config.MatchPollIntervalSeconds = 1.0;
			Config.MatchWaitTimeoutSeconds = 60.0;
			Config.ResultPollIntervalSeconds = 1.0;
			Config.ResultWaitTimeoutSeconds = 10.0;
			Config.ReconnectPollIntervalSeconds = 5.0;
			Flow = MakeUnique<FVeyraClientFlow>(Backend, Host, Config);
		}

		EVeyraClientState State() const { return Flow->GetSnapshot().State; }

		void Advance(double Seconds)
		{
			Host.Clock += Seconds;
			Flow->Tick();
		}

		/** Starts, reads a launch code and redeems it: the flow then asks for the player's match. */
		bool SignIn()
		{
			Flow->Start();
			Host.Input.Add(ExampleCredential(TEXT("vlc_"), TEXT('A')));
			Flow->Tick();
			return Backend.Answer(TEXT("POST"), TEXT("/v1/game-sessions"), 200, SessionBody()) && State() == EVeyraClientState::Loading;
		}

		/** Signs in with no match, no select and the tutorial completed. */
		bool ReachShell()
		{
			return SignIn() && Backend.Answer(TEXT("GET"), TEXT("/v1/me/match"), 200, NoMatch) && Backend.Answer(TEXT("GET"), TEXT("/v1/me/select"), 200, NoSelect)
				&& Backend.Answer(TEXT("GET"), TEXT("/v1/me/profile"), 200, ProfileBody(true)) && State() == EVeyraClientState::Shell;
		}

		/** From the shell into a practice select, with the available Vanguards read. */
		bool ReachSelect()
		{
			return ReachShell() && Flow->StartPractice() && Backend.Answer(TEXT("POST"), TEXT("/v1/practice"), 201, SelectBody(TEXT("picking")))
				&& Backend.Answer(TEXT("GET"), TEXT("/v1/me/vanguards"), 200, VanguardsBody) && State() == EVeyraClientState::Selecting;
		}

		/** Locks Oriel; the match's server is ready at once, and the game joins it. */
		bool ReachMatch()
		{
			return ReachSelect() && Flow->LockVanguard(TEXT("oriel"))
				&& Backend.Answer(TEXT("POST"), TEXT("/v1/me/select/lock"), 200, SelectBody(TEXT("started"), TEXT("oriel"), TEXT("oriel"), MatchId))
				&& Backend.Answer(TEXT("GET"), TEXT("/v1/me/match"), 200, ReadyMatch()) && State() == EVeyraClientState::Connecting
				&& (Flow->NotifyWorld(EVeyraClientWorld::Match), State() == EVeyraClientState::InMatch);
		}

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
