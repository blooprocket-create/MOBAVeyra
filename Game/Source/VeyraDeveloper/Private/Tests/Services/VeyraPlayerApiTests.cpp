// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER

#include "Backend/VeyraBackendProtocol.h"

namespace VeyraPlayerApiTests
{
	const TCHAR* const AccountId = TEXT("11111111-2222-4333-8444-555555555555");
	const TCHAR* const SelectId = TEXT("22222222-3333-4444-8555-666666666666");
	const TCHAR* const MatchId = TEXT("33333333-4444-4555-8666-777777777777");

	FString Seat(const TCHAR* Name, const TCHAR* Side, bool bYou, const TCHAR* Hover, const TCHAR* Locked)
	{
		return FString::Printf(TEXT("{\"displayName\":\"%s\",\"side\":\"%s\",\"you\":%s,\"hover\":%s,\"locked\":%s}"), Name, Side, bYou ? TEXT("true") : TEXT("false"),
			Hover, Locked);
	}

	FString Select(const TCHAR* State, const FString& Seats, const TCHAR* Match, const TCHAR* CancelReason, const TCHAR* Remaining = TEXT("12.5"))
	{
		return FString::Printf(TEXT("{\"select\":{\"id\":\"%s\",\"kind\":\"practice\",\"mode\":\"custom_practice\",\"state\":\"%s\",")
							   TEXT("\"deadline\":\"2026-09-27T12:00:30Z\",\"remainingSeconds\":%s,\"seats\":[%s],\"matchId\":%s,\"cancelReason\":%s}}"),
			SelectId, State, Remaining, *Seats, Match, CancelReason);
	}

	FString Outcome(const TCHAR* State, const TCHAR* VanguardId, const TCHAR* FailureReason, const TCHAR* Result)
	{
		return FString::Printf(TEXT("{\"match\":{\"id\":\"%s\",\"mode\":\"custom_practice\",\"rules\":\"practice\",\"state\":\"%s\",\"side\":\"A\",")
							   TEXT("\"vanguardId\":%s,\"failureReason\":%s,\"result\":%s}}"),
			MatchId, State, VanguardId, FailureReason, Result);
	}

	// Veyra.Services.PlayerApi.*: reading the player routes the client-state coordinator uses
	// (ADR-010 §3, §6–8). A problem never quotes the body.
	TEST_CLASS(PlayerApi, "Veyra.Services")
	{
		TEST_METHOD(ReadsAProfile)
		{
			VeyraBackendProtocol::FProfile Profile;
			FString Problem;
			const FString New = FString::Printf(TEXT("{\"account\":{\"id\":\"%s\",\"displayName\":\"DevOne\"},\"tutorial\":{\"completed\":false,\"starterVanguardId\":null}}"), AccountId);
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseProfile(New, Profile, Problem), Problem));
			ASSERT_THAT(IsFalse(Profile.bTutorialCompleted));
			ASSERT_THAT(IsTrue(Profile.StarterVanguardId.IsEmpty()));
			ASSERT_THAT(AreEqual(Profile.DisplayName, FString(TEXT("DevOne"))));

			const FString Done = FString::Printf(TEXT("{\"account\":{\"id\":\"%s\",\"displayName\":\"DevOne\"},\"tutorial\":{\"completed\":true,\"starterVanguardId\":\"oriel\"}}"), AccountId);
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseProfile(Done, Profile, Problem), Problem));
			ASSERT_THAT(IsTrue(Profile.bTutorialCompleted));
			ASSERT_THAT(AreEqual(Profile.StarterVanguardId, FString(TEXT("oriel"))));
		}

		TEST_METHOD(RefusesAnythingElseAsAProfile)
		{
			const TArray<FString> Bodies = {
				TEXT("{}"),
				FString::Printf(TEXT("{\"account\":{\"id\":\"%s\",\"displayName\":\"DevOne\"}}"), AccountId),
				FString::Printf(TEXT("{\"account\":{\"id\":\"%s\",\"displayName\":\"DevOne\"},\"tutorial\":{\"completed\":\"yes\",\"starterVanguardId\":null}}"), AccountId),
				FString::Printf(TEXT("{\"account\":{\"id\":\"%s\",\"displayName\":\"DevOne\"},\"tutorial\":{\"completed\":true,\"starterVanguardId\":\"Oriel!\"}}"), AccountId),
				TEXT("{\"account\":{\"id\":\"no\",\"displayName\":\"DevOne\"},\"tutorial\":{\"completed\":false,\"starterVanguardId\":null}}"),
			};
			for (const FString& Body : Bodies)
			{
				VeyraBackendProtocol::FProfile Profile;
				FString Problem;
				ASSERT_THAT(IsFalse(VeyraBackendProtocol::ParseProfile(Body, Profile, Problem), Body));
				ASSERT_THAT(IsFalse(Problem.IsEmpty()));
			}
		}

		TEST_METHOD(ReadsTheVanguardsAPlayerMayPick)
		{
			VeyraBackendProtocol::FVanguardAccess Access;
			FString Problem;
			const TCHAR* const Body = TEXT("{\"owned\":[\"oriel\"],\"rotation\":[\"cairn\",\"bryn\"],\"available\":[\"cairn\",\"oriel\",\"bryn\"],\"starters\":[\"cairn\",\"oriel\"]}");
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseVanguardAccess(Body, Access, Problem), Problem));
			ASSERT_THAT(IsTrue(Access.Owned == TArray<FString>{ TEXT("oriel") }));
			ASSERT_THAT(IsTrue(Access.Available == (TArray<FString>{ TEXT("cairn"), TEXT("oriel"), TEXT("bryn") })));
			ASSERT_THAT(AreEqual(Access.Starters.Num(), 2));

			for (const TCHAR* Bad : { TEXT("{\"owned\":[],\"rotation\":[],\"available\":[]}"),
					 TEXT("{\"owned\":[],\"rotation\":[],\"available\":[\"Cairn\"],\"starters\":[]}"),
					 TEXT("{\"owned\":null,\"rotation\":[],\"available\":[],\"starters\":[]}"),
					 TEXT("{\"owned\":[7],\"rotation\":[],\"available\":[],\"starters\":[]}") })
			{
				ASSERT_THAT(IsFalse(VeyraBackendProtocol::ParseVanguardAccess(Bad, Access, Problem), Bad));
			}
		}

		TEST_METHOD(ReadsASelectAsItsPlayerSeesIt)
		{
			const FString Seats = Seat(TEXT("DevOne"), TEXT("A"), true, TEXT("\"oriel\""), TEXT("null")) + TEXT(",")
				+ Seat(TEXT("DevTwo"), TEXT("B"), false, TEXT("null"), TEXT("\"cairn\""));
			TOptional<VeyraBackendProtocol::FSelect> Read;
			FString Problem;
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseSelect(Select(TEXT("picking"), Seats, TEXT("null"), TEXT("null")), Read, Problem), Problem));
			ASSERT_THAT(IsTrue(Read.IsSet()));
			ASSERT_THAT(AreEqual(Read->Id, FString(SelectId)));
			ASSERT_THAT(IsTrue(Read->State == VeyraBackendProtocol::ESelectState::Picking));
			ASSERT_THAT(IsTrue(Read->RemainingSeconds == 12.5));
			ASSERT_THAT(AreEqual(Read->Seats.Num(), 2));
			ASSERT_THAT(AreEqual(Read->FindYou()->Hover, FString(TEXT("oriel"))));
			ASSERT_THAT(AreEqual(Read->Seats[1].Locked, FString(TEXT("cairn"))));

			const FString Solo = Seat(TEXT("DevOne"), TEXT("A"), true, TEXT("\"oriel\""), TEXT("\"oriel\""));
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseSelect(Select(TEXT("started"), Solo, *FString::Printf(TEXT("\"%s\""), MatchId), TEXT("null"), TEXT("0")), Read, Problem), Problem));
			ASSERT_THAT(AreEqual(Read->MatchId, FString(MatchId)));
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseSelect(Select(TEXT("cancelled"), Solo, TEXT("null"), TEXT("\"timed_out\""), TEXT("0")), Read, Problem), Problem));
			ASSERT_THAT(AreEqual(Read->CancelReason, FString(TEXT("timed_out"))));

			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseSelect(TEXT("{\"select\":null}"), Read, Problem), Problem));
			ASSERT_THAT(IsFalse(Read.IsSet()));
		}

		TEST_METHOD(RefusesASelectThatDoesNotFit)
		{
			const FString You = Seat(TEXT("DevOne"), TEXT("A"), true, TEXT("null"), TEXT("null"));
			const FString Them = Seat(TEXT("DevTwo"), TEXT("B"), false, TEXT("null"), TEXT("null"));
			const FString Match = FString::Printf(TEXT("\"%s\""), MatchId);
			const TArray<FString> Bodies = {
				TEXT("{}"),
				Select(TEXT("thinking"), You, TEXT("null"), TEXT("null")),
				Select(TEXT("picking"), Them, TEXT("null"), TEXT("null")),
				Select(TEXT("picking"), You + TEXT(",") + You, TEXT("null"), TEXT("null")),
				Select(TEXT("started"), You, TEXT("null"), TEXT("null")),
				Select(TEXT("picking"), You, *Match, TEXT("null")),
				Select(TEXT("cancelled"), You, TEXT("null"), TEXT("null")),
				Select(TEXT("picking"), You, TEXT("null"), TEXT("null"), TEXT("-1")),
				Select(TEXT("picking"), Seat(TEXT("DevOne"), TEXT("C"), true, TEXT("null"), TEXT("null")), TEXT("null"), TEXT("null")),
				Select(TEXT("picking"), Seat(TEXT("DevOne"), TEXT("A"), true, TEXT("\"../x\""), TEXT("null")), TEXT("null"), TEXT("null")),
			};
			for (const FString& Body : Bodies)
			{
				TOptional<VeyraBackendProtocol::FSelect> Read;
				FString Problem;
				ASSERT_THAT(IsFalse(VeyraBackendProtocol::ParseSelect(Body, Read, Problem), Body));
				ASSERT_THAT(IsFalse(Problem.IsEmpty()));
			}
		}

		TEST_METHOD(ReadsAMatchOutcome)
		{
			VeyraBackendProtocol::FMatchOutcome Read;
			FString Problem;
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseMatchOutcome(Outcome(TEXT("ready"), TEXT("\"oriel\""), TEXT("null"), TEXT("null")), Read, Problem), Problem));
			ASSERT_THAT(IsTrue(Read.IsActive()));
			ASSERT_THAT(IsFalse(Read.bHasResult));

			const TCHAR* const Result = TEXT("{\"endReason\":\"host_ended\",\"winner\":null,\"durationSeconds\":42.5,\"joined\":true,\"connectedAtEnd\":false}");
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseMatchOutcome(Outcome(TEXT("ended"), TEXT("\"oriel\""), TEXT("null"), Result), Read, Problem), Problem));
			ASSERT_THAT(IsFalse(Read.IsActive()));
			ASSERT_THAT(IsTrue(Read.bHasResult));
			ASSERT_THAT(AreEqual(Read.EndReason, FString(TEXT("host_ended"))));
			ASSERT_THAT(IsTrue(Read.Winner.IsEmpty()));
			ASSERT_THAT(IsTrue(Read.DurationSeconds == 42.5));
			ASSERT_THAT(IsTrue(Read.bJoined && !Read.bConnectedAtEnd));
			ASSERT_THAT(AreEqual(Read.Rules, FString(TEXT("practice"))));

			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseMatchOutcome(Outcome(TEXT("failed"), TEXT("\"oriel\""), TEXT("\"server_exited\""), TEXT("null")), Read, Problem), Problem));
			ASSERT_THAT(AreEqual(Read.FailureReason, FString(TEXT("server_exited"))));
		}

		TEST_METHOD(RefusesAnythingElseAsAMatchOutcome)
		{
			const TArray<FString> Bodies = {
				TEXT("{\"match\":null}"),
				Outcome(TEXT("ended"), TEXT("7"), TEXT("null"), TEXT("null")),
				Outcome(TEXT("ended"), TEXT("\"oriel\""), TEXT("null"), TEXT("{\"endReason\":\"host_ended\"}")),
				Outcome(TEXT("ended"), TEXT("\"oriel\""), TEXT("null"), TEXT("{\"endReason\":\"host_ended\",\"winner\":\"C\",\"durationSeconds\":1,\"joined\":true,\"connectedAtEnd\":true}")),
				Outcome(TEXT("ended"), TEXT("\"oriel\""), TEXT("null"), TEXT("[]")),
			};
			for (const FString& Body : Bodies)
			{
				VeyraBackendProtocol::FMatchOutcome Read;
				FString Problem;
				ASSERT_THAT(IsFalse(VeyraBackendProtocol::ParseMatchOutcome(Body, Read, Problem), Body));
				ASSERT_THAT(IsFalse(Problem.IsEmpty()));
			}
		}

		TEST_METHOD(WritesAVanguardChoice)
		{
			ASSERT_THAT(AreEqual(VeyraBackendProtocol::BuildVanguardBody(TEXT("oriel")), FString(TEXT("{\"vanguardId\":\"oriel\"}"))));
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::IsContentId(TEXT("test_vanguard"))));
			ASSERT_THAT(IsFalse(VeyraBackendProtocol::IsContentId(TEXT("Test"))));
			ASSERT_THAT(IsFalse(VeyraBackendProtocol::IsContentId(TEXT("a__b"))));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
