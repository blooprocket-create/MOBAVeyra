// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER

#include "Tests/Services/VeyraClientFlowTestRig.h"

namespace VeyraClientFlowTests
{
	// Veyra.Services.AccountSettingsSync.*: the player's account settings between this client and the
	// backend (ADR-024 §1), through the coordinator on the fake backend.
	TEST_CLASS(AccountSettingsSync, "Veyra.Services")
	{
		/** Before the rig, which outlives none of what it was given. */
		FFakeAccountSettingsCache Cache;
		FClientFlowTestRig Rig;
		FFlowTestBackend& Backend = Rig.Backend;
		TUniquePtr<FVeyraClientFlow>& Flow = Rig.Flow;

		const TCHAR* SettingsPath = TEXT("/v1/account/settings");
		const TCHAR* Speed = TEXT("camera_move_speed");

		BEFORE_EACH()
		{
			Flow->SyncAccountSettings(Cache);
		}

		/** The document as the backend writes it, with one setting. */
		FString Document(int64 Revision, const TCHAR* Value) const
		{
			return FString::Printf(TEXT("{\"schemaVersion\":1,\"revision\":%lld,\"values\":{\"%s\":\"%s\"}}"), Revision, Speed, Value);
		}

		/** The backend's refusal of a stale send, with its document. */
		FString Conflict(int64 Revision, const TCHAR* Value) const
		{
			return FString::Printf(TEXT("{\"error\":\"settings_conflict\",\"current\":%s}"), *Document(Revision, Value));
		}

		/** The flow went on from sign-in: it asks for the player's match. */
		bool Resumed() const { return Backend.Find(TEXT("GET"), TEXT("/v1/me/match")) != nullptr; }

		/** A send of the settings on its way, with its revision and value; false for none. */
		bool Sending(int64 Revision, const TCHAR* Value) const
		{
			const FFlowTestBackend::FRequest* Put = Backend.Find(TEXT("PUT"), SettingsPath);
			return Put && Put->Body.Contains(FString::Printf(TEXT("\"revision\":%lld"), Revision)) && Put->Body.Contains(FString::Printf(TEXT("\"%s\":\"%s\""), Speed, Value));
		}

		bool Sends() const { return Backend.Find(TEXT("PUT"), SettingsPath) != nullptr; }

		/** Into the shell, the account's settings read as Revision with Value. */
		bool ReachShell(int64 Revision, const TCHAR* Value)
		{
			Rig.AccountSettingsAnswer = Document(Revision, Value);
			return Rig.ReachShell();
		}

		/** The player changes the speed, and the next frame sees it. */
		void Change(const TCHAR* Value)
		{
			Cache.Change(Speed, Value);
			Rig.Advance(0.0);
		}

		TEST_METHOD(SignInTakesTheAccountsSettingsBeforeGoingOn)
		{
			ASSERT_THAT(IsTrue(Rig.SignIn()));
			const FFlowTestBackend::FRequest* Read = Backend.Find(TEXT("GET"), SettingsPath);
			ASSERT_THAT(IsTrue(Read && Read->Credential == GameSession() && !Resumed(), TEXT("the settings come first, with the game session")));
			ASSERT_THAT(AreEqual(FString(AccountId), Cache.Account, TEXT("the account's cached settings apply at once")));

			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), SettingsPath, 200, Document(3, TEXT("70")))));
			ASSERT_THAT(IsTrue(Cache.Document.Revision == 3 && Cache.Document.Values.FindRef(Speed) == TEXT("70") && !Cache.Document.bUnsent));
			ASSERT_THAT(IsTrue(Resumed() && !Sends()));
		}

		TEST_METHOD(ChangesMadeWhileOfflineAreSentOnSignIn)
		{
			Cache.Document.Revision = 3;
			Cache.Change(Speed, TEXT("70"));
			ASSERT_THAT(IsTrue(Rig.SignIn() && Backend.Answer(TEXT("GET"), SettingsPath, 200, Document(3, TEXT("50")))));
			ASSERT_THAT(IsTrue(Sending(3, TEXT("70")) && Resumed(), TEXT("nothing changed elsewhere: this device's changes go, and so does the player")));

			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("PUT"), SettingsPath, 200, Document(4, TEXT("70")))));
			ASSERT_THAT(IsTrue(Cache.Document.Revision == 4 && !Cache.Document.bUnsent));
		}

		TEST_METHOD(WhenBothChangedThePlayerChoosesBeforeTheShell)
		{
			Cache.Document.Revision = 3;
			Cache.Change(Speed, TEXT("70"));
			ASSERT_THAT(IsTrue(Rig.SignIn() && Backend.Answer(TEXT("GET"), SettingsPath, 200, Document(5, TEXT("20")))));
			ASSERT_THAT(IsTrue(Flow->GetSnapshot().bSettingsConflict && Flow->CanIssue(EVeyraClientIntent::ResolveSettingsConflict)));
			ASSERT_THAT(IsTrue(!Resumed() && !Sends(), TEXT("neither is overwritten, and the shell waits")));

			ASSERT_THAT(IsTrue(Flow->ResolveSettingsConflict(/*bKeepThisDevice*/ true)));
			ASSERT_THAT(IsTrue(Sending(5, TEXT("70")) && Resumed() && !Flow->GetSnapshot().bSettingsConflict, TEXT("this device's go over the account's")));
			ASSERT_THAT(IsFalse(Flow->ResolveSettingsConflict(true), TEXT("the choice is made once")));
		}

		TEST_METHOD(TakingTheAccountsSettingsDropsThisDevicesChanges)
		{
			Cache.Document.Revision = 3;
			Cache.Change(Speed, TEXT("70"));
			ASSERT_THAT(IsTrue(Rig.SignIn() && Backend.Answer(TEXT("GET"), SettingsPath, 200, Document(5, TEXT("20")))));
			ASSERT_THAT(IsTrue(Flow->ResolveSettingsConflict(/*bKeepThisDevice*/ false)));
			ASSERT_THAT(IsTrue(Cache.Document.Revision == 5 && Cache.Document.Values.FindRef(Speed) == TEXT("20") && !Cache.Document.bUnsent));
			ASSERT_THAT(IsTrue(Resumed() && !Sends()));
		}

		TEST_METHOD(ChangesAreSentOnceTheySettle)
		{
			ASSERT_THAT(IsTrue(ReachShell(1, TEXT("50"))));
			Change(TEXT("60"));
			Rig.Advance(1.0);
			Change(TEXT("70"));
			Rig.Advance(1.0);
			ASSERT_THAT(IsFalse(Sends(), TEXT("a drag sends once, after it settles")));
			Rig.Advance(0.5);
			ASSERT_THAT(IsTrue(Sending(1, TEXT("70"))));

			// A change made while the send is on its way goes next, based on the revision the send made.
			Change(TEXT("80"));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("PUT"), SettingsPath, 200, Document(2, TEXT("70")))));
			ASSERT_THAT(IsTrue(Cache.Document.Revision == 2 && Cache.Document.bUnsent));
			Rig.Advance(0.0);
			Rig.Advance(1.5);
			ASSERT_THAT(IsTrue(Sending(2, TEXT("80"))));
		}

		TEST_METHOD(AStaleSendAsksThePlayerWhereverTheyAre)
		{
			ASSERT_THAT(IsTrue(ReachShell(1, TEXT("50"))));
			Change(TEXT("70"));
			Rig.Advance(1.5);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("PUT"), SettingsPath, 409, Conflict(4, TEXT("20")))));
			ASSERT_THAT(IsTrue(Flow->GetSnapshot().bSettingsConflict && Rig.State() == EVeyraClientState::Shell));
			Rig.Advance(30.0);
			ASSERT_THAT(IsFalse(Sends(), TEXT("nothing goes until the player chooses")));

			ASSERT_THAT(IsTrue(Flow->ResolveSettingsConflict(/*bKeepThisDevice*/ false)));
			ASSERT_THAT(IsTrue(Cache.Document.Revision == 4 && Cache.Document.Values.FindRef(Speed) == TEXT("20") && !Flow->GetSnapshot().bSettingsConflict));
			ASSERT_THAT(IsTrue(Rig.State() == EVeyraClientState::Shell && !Sends()));
		}

		TEST_METHOD(ARefusedSendOfSameValuesOnlyCatchesUp)
		{
			ASSERT_THAT(IsTrue(ReachShell(1, TEXT("50"))));
			Change(TEXT("70"));
			Rig.Advance(1.5);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("PUT"), SettingsPath, 409, Conflict(4, TEXT("70")))));
			ASSERT_THAT(IsTrue(!Flow->GetSnapshot().bSettingsConflict && Cache.Document.Revision == 4 && !Cache.Document.bUnsent,
				TEXT("another machine saved the same: nothing to choose")));
		}

		TEST_METHOD(SettingsNeverStopThePlayer)
		{
			TestRunner->AddExpectedMessagePlain(TEXT("the backend's could not be read"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
			TestRunner->AddExpectedMessagePlain(TEXT("sending the account settings failed"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
			TestRunner->AddExpectedMessagePlain(TEXT("the backend refused the account settings"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1);
			Cache.Document.Revision = 1;
			Cache.Change(Speed, TEXT("70"));
			ASSERT_THAT(IsTrue(Rig.SignIn() && Backend.Answer(TEXT("GET"), SettingsPath, 0)));
			ASSERT_THAT(IsTrue(Resumed() && Cache.Document.Values.FindRef(Speed) == TEXT("70"), TEXT("no answer: this device's settings stay, and the player goes on")));

			Rig.Advance(0.0);
			ASSERT_THAT(IsTrue(Sending(1, TEXT("70")) && Backend.Answer(TEXT("PUT"), SettingsPath, 0)));
			Rig.Advance(14.0);
			ASSERT_THAT(IsFalse(Sends()));
			Rig.Advance(1.0);
			ASSERT_THAT(IsTrue(Sending(1, TEXT("70")), TEXT("an unanswered send is tried again later")));

			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("PUT"), SettingsPath, 413, ErrorBody(TEXT("settings_too_large")))));
			Rig.Advance(60.0);
			ASSERT_THAT(IsFalse(Sends(), TEXT("settings the backend refuses as they are go again only after another change")));
			Change(TEXT("60"));
			Rig.Advance(1.5);
			ASSERT_THAT(IsTrue(Sending(1, TEXT("60"))));
		}

		TEST_METHOD(ARefusedSessionEndsIt)
		{
			TestRunner->AddExpectedMessagePlain(TEXT("VeyraClientFlow: the backend ended the game session"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
			ASSERT_THAT(IsTrue(Rig.SignIn() && Backend.Answer(TEXT("GET"), SettingsPath, 401, ErrorBody(TEXT("invalid_credentials")))));
			ASSERT_THAT(IsTrue(Rig.State() == EVeyraClientState::SessionEnded && !Resumed()));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
