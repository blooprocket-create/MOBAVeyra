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
							   TEXT("\"deadline\":\"2026-09-27T12:00:30Z\",\"remainingSeconds\":%s,\"pickSeconds\":30,\"seats\":[%s],\"matchId\":%s,\"cancelReason\":%s}}"),
			SelectId, State, Remaining, *Seats, Match, CancelReason);
	}

	FString Outcome(const TCHAR* State, const TCHAR* VanguardId, const TCHAR* FailureReason, const TCHAR* Result)
	{
		return FString::Printf(TEXT("{\"match\":{\"id\":\"%s\",\"mode\":\"custom_practice\",\"rules\":\"practice\",\"state\":\"%s\",\"side\":\"A\",")
							   TEXT("\"vanguardId\":%s,\"failureReason\":%s,\"result\":%s}}"),
			MatchId, State, VanguardId, FailureReason, Result);
	}

	/** A verified result's scoreboard line, as the backend returns it. */
	const TCHAR* const ScoreboardLine = TEXT("{\"side\":\"A\",\"name\":\"DevOne\",\"vanguardId\":\"cairn\",\"you\":true,\"statistics\":{\"kills\":1,\"deaths\":0,\"assists\":2,\"level\":7,\"minionKills\":40,\"jungleKills\":0,\"wellsSecured\":0,\"wellFinalHits\":0,\"wardsPlaced\":1,\"wardsDestroyed\":0,\"vanguardDamage\":900.5,\"damageShielded\":0,\"selfHealing\":0,\"teammateHealing\":0,\"goldEarned\":2100,\"towerDamage\":0,\"wellDamage\":0,\"damageDealt\":{\"physical\":3000,\"magic\":0,\"true\":0},\"damageTaken\":{\"physical\":800,\"magic\":0,\"true\":0},\"crowdControl\":{\"stun\":1.5,\"slow\":0,\"total\":1.5},\"goldBySource\":{\"starting\":500,\"kills\":300,\"assists\":0,\"minions\":1200,\"jungle\":0,\"objectives\":0,\"wards\":0,\"passive\":100}},\"items\":[\"timing_coil\",\"\"],\"fluxSpells\":[\"blink\",\"\"]}");

	/** An ended match whose result's scoreboard is the one line Line. */
	FString ScoredOutcome(const FString& Line)
	{
		const FString Result = FString::Printf(
			TEXT("{\"endReason\":\"host_ended\",\"winner\":null,\"durationSeconds\":1,\"joined\":true,\"connectedAtEnd\":true,\"players\":[%s]}"), *Line);
		return Outcome(TEXT("ended"), TEXT("\"oriel\""), TEXT("null"), *Result);
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
			ASSERT_THAT(IsTrue(Read->PickSeconds == 30.0, TEXT("the pick timer's full length, for its bars")));
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
				Select(TEXT("picking"), You, TEXT("null"), TEXT("null")).Replace(TEXT("\"pickSeconds\":30,"), TEXT("")),
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
			ASSERT_THAT(IsFalse(Read.bHasScoreboard, TEXT("an older backend's result has no scoreboard")));

			// With a scoreboard (ADR-017 §5).
			const FString Scored = FString::Printf(TEXT("{\"endReason\":\"host_ended\",\"winner\":null,\"durationSeconds\":42.5,\"joined\":true,\"connectedAtEnd\":true,\"players\":[%s]}"), ScoreboardLine);
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseMatchOutcome(Outcome(TEXT("ended"), TEXT("\"oriel\""), TEXT("null"), *Scored), Read, Problem), Problem));
			ASSERT_THAT(IsTrue(Read.bHasScoreboard && Read.Players.Num() == 1 && Read.Players[0].bYou && Read.Players[0].Name == TEXT("DevOne")));
			const FVeyraPlayerStatistics& Statistics = Read.Players[0].Statistics;
			ASSERT_THAT(IsTrue(Statistics.Kills == 1 && Statistics.Assists == 2 && Statistics.Level == 7 && Statistics.CrowdControl.Stun == 1.5 && Statistics.GoldBySource.Passive == 100.0));
			ASSERT_THAT(IsTrue(Statistics.Items.Num() == 2 && Statistics.Items[0].ToString() == TEXT("timing_coil") && !Statistics.Items[1].IsValid()));
			ASSERT_THAT(IsTrue(Statistics.FluxSpells.Num() == 2 && !Statistics.FluxSpells[1].IsValid()));
			const FString Unscored = TEXT("{\"endReason\":\"host_ended\",\"winner\":null,\"durationSeconds\":42.5,\"joined\":true,\"connectedAtEnd\":true,\"players\":null}");
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseMatchOutcome(Outcome(TEXT("ended"), TEXT("\"oriel\""), TEXT("null"), *Unscored), Read, Problem), Problem));
			ASSERT_THAT(IsTrue(!Read.bHasScoreboard && Read.Players.IsEmpty()));

			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseMatchOutcome(Outcome(TEXT("failed"), TEXT("\"oriel\""), TEXT("\"server_exited\""), TEXT("null")), Read, Problem), Problem));
			ASSERT_THAT(AreEqual(Read.FailureReason, FString(TEXT("server_exited"))));
		}

		TEST_METHOD(ReadsMatchHistoryAndBuildsItsPath)
		{
			ASSERT_THAT(AreEqual(VeyraBackendProtocol::HistoryPath({}, FString()), FString(TEXT("/v1/me/matches"))));
			VeyraBackendProtocol::FHistoryFilter Filter;
			Filter.VanguardId = TEXT("cairn");
			Filter.Mode = TEXT("casual_select");
			Filter.Outcome = TEXT("no_contest");
			ASSERT_THAT(AreEqual(VeyraBackendProtocol::HistoryPath(Filter, TEXT("abc_-1")),
				FString(TEXT("/v1/me/matches?vanguard=cairn&mode=casual_select&outcome=no_contest&cursor=abc_-1"))));

			const FString Body = TEXT("{\"matches\":[{\"id\":\"33333333-4444-4555-8666-777777777777\",\"mode\":\"custom_practice\",\"rules\":\"practice\",")
				TEXT("\"endedAt\":\"2026-09-29T10:03:12Z\",\"durationSeconds\":95,\"side\":\"A\",\"vanguardId\":null,\"outcome\":\"no_contest\"}],\"next\":\"bmV4dA\"}");
			VeyraBackendProtocol::FHistoryPage Page;
			FString Problem;
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseHistoryPage(Body, Page, Problem), Problem));
			ASSERT_THAT(IsTrue(Page.Entries.Num() == 1 && Page.Entries[0].Rules == TEXT("practice") && Page.Entries[0].VanguardId.IsEmpty() && Page.Next == TEXT("bmV4dA")));
			ASSERT_THAT(IsTrue(Page.Entries[0].EndedAt == FDateTime(2026, 9, 29, 10, 3, 12)));

			for (const FString& Bad : TArray<FString>{
					 TEXT("{\"matches\":null,\"next\":null}"),
					 Body.Replace(TEXT("no_contest"), TEXT("draw")),
					 Body.Replace(TEXT("2026-09-29T10:03:12Z"), TEXT("yesterday")),
					 Body.Replace(TEXT("\"bmV4dA\""), TEXT("\"not a cursor!\"")),
				 })
			{
				ASSERT_THAT(IsFalse(VeyraBackendProtocol::ParseHistoryPage(Bad, Page, Problem), Bad));
			}
		}

		TEST_METHOD(RefusesAnythingElseAsAMatchOutcome)
		{
			const TArray<FString> Bodies = {
				TEXT("{\"match\":null}"),
				Outcome(TEXT("ended"), TEXT("7"), TEXT("null"), TEXT("null")),
				Outcome(TEXT("ended"), TEXT("\"oriel\""), TEXT("null"), TEXT("{\"endReason\":\"host_ended\"}")),
				Outcome(TEXT("ended"), TEXT("\"oriel\""), TEXT("null"), TEXT("{\"endReason\":\"host_ended\",\"winner\":\"C\",\"durationSeconds\":1,\"joined\":true,\"connectedAtEnd\":true}")),
				Outcome(TEXT("ended"), TEXT("\"oriel\""), TEXT("null"), TEXT("[]")),
				ScoredOutcome(TEXT("{}")),
				ScoredOutcome(FString(ScoreboardLine).Replace(TEXT("\"kills\":1"), TEXT("\"kills\":-1"))),
				ScoredOutcome(FString(ScoreboardLine).Replace(TEXT("\"level\":7"), TEXT("\"level\":7.5"))),
				ScoredOutcome(FString(ScoreboardLine).Replace(TEXT("\"slow\":0"), TEXT("\"slow\":\"0\""))),
				ScoredOutcome(FString(ScoreboardLine).Replace(TEXT("\"fluxSpells\":[\"blink\",\"\"]"), TEXT("\"fluxSpells\":[\"blink\"]"))),
				ScoredOutcome(FString(ScoreboardLine).Replace(TEXT("\"timing_coil\""), TEXT("\"Timing Coil\""))),
				ScoredOutcome(FString(ScoreboardLine).Replace(TEXT("\"name\":\"DevOne\""), TEXT("\"name\":\"\""))),
				Outcome(TEXT("ended"), TEXT("\"oriel\""), TEXT("null"),
					TEXT("{\"endReason\":\"host_ended\",\"winner\":null,\"durationSeconds\":1,\"joined\":true,\"connectedAtEnd\":true,\"players\":{}}")),
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

	const TCHAR* const PartyId = TEXT("44444444-5555-4666-8777-888888888888");
	const TCHAR* const OtherAccountId = TEXT("66666666-7777-4888-8999-aaaaaaaaaaaa");
	const TCHAR* const FoundId = TEXT("55555555-6666-4777-8888-999999999999");

	FString Member(const TCHAR* Id, const TCHAR* Name, bool bReady, bool bLeader)
	{
		return FString::Printf(TEXT("{\"accountId\":\"%s\",\"displayName\":\"%s\",\"ready\":%s,\"leader\":%s}"), Id, Name, bReady ? TEXT("true") : TEXT("false"),
			bLeader ? TEXT("true") : TEXT("false"));
	}

	FString Party(const TCHAR* Mode, const TCHAR* Status, const FString& Members, const TCHAR* QueuedSeconds = TEXT("0"))
	{
		return FString::Printf(TEXT("{\"party\":{\"id\":\"%s\",\"mode\":\"%s\",\"privacy\":\"private\",\"status\":\"%s\",\"queuedSeconds\":%s,\"members\":[%s]}}"), PartyId, Mode,
			Status, QueuedSeconds, *Members);
	}

	FString Found(const TCHAR* State, const TCHAR* You, const TCHAR* Accepted, const TCHAR* Total, const TCHAR* SelectIdValue, const TCHAR* Reason)
	{
		return FString::Printf(TEXT("{\"matchFound\":{\"id\":\"%s\",\"mode\":\"casual_select\",\"state\":\"%s\",\"deadline\":\"2026-09-27T12:00:15Z\",")
							   TEXT("\"remainingSeconds\":9.5,\"accepted\":%s,\"total\":%s,\"you\":\"%s\",\"selectId\":%s,\"abandonReason\":%s}}"),
			FoundId, State, Accepted, Total, You, SelectIdValue, Reason);
	}

	// Veyra.Services.MatchmakingApi.*: reading the modes, the party and Match Found, which the
	// client-state coordinator polls (ADR-010 §10; Parties & Social Bible §2–3). A problem never
	// quotes the body.
	TEST_CLASS(MatchmakingApi, "Veyra.Services")
	{
		TEST_METHOD(ReadsTheModes)
		{
			TArray<VeyraBackendProtocol::FModeInfo> Modes;
			FString Problem;
			const TCHAR* const Body = TEXT("{\"modes\":[{\"id\":\"casual_select\",\"enabled\":true,\"humanPlayersPerTeam\":1,\"matchmaking\":\"casualSelect\"},")
									  TEXT("{\"id\":\"ranked\",\"enabled\":false,\"humanPlayersPerTeam\":5,\"matchmaking\":\"notImplemented\"}]}");
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseModes(Body, Modes, Problem), Problem));
			ASSERT_THAT(AreEqual(Modes.Num(), 2));
			ASSERT_THAT(AreEqual(Modes[0].Id, FString(TEXT("casual_select"))));
			ASSERT_THAT(IsTrue(Modes[0].bEnabled && Modes[0].bMatchmade));
			ASSERT_THAT(AreEqual(Modes[0].HumanPlayersPerTeam, 1));
			ASSERT_THAT(IsFalse(Modes[1].bEnabled || Modes[1].bMatchmade));

			for (const TCHAR* Bad : { TEXT("{}"), TEXT("{\"modes\":null}"),
					 TEXT("{\"modes\":[{\"id\":\"Casual\",\"enabled\":true,\"humanPlayersPerTeam\":1,\"matchmaking\":\"casualSelect\"}]}"),
					 TEXT("{\"modes\":[{\"id\":\"casual_select\",\"enabled\":true,\"humanPlayersPerTeam\":0,\"matchmaking\":\"casualSelect\"}]}"),
					 TEXT("{\"modes\":[{\"id\":\"casual_select\",\"enabled\":\"yes\",\"humanPlayersPerTeam\":1,\"matchmaking\":\"casualSelect\"}]}"),
					 TEXT("{\"modes\":[{\"id\":\"casual_select\",\"enabled\":true,\"humanPlayersPerTeam\":1}]}") })
			{
				ASSERT_THAT(IsFalse(VeyraBackendProtocol::ParseModes(Bad, Modes, Problem), Bad));
				ASSERT_THAT(IsFalse(Problem.IsEmpty()));
			}
		}

		TEST_METHOD(ReadsAParty)
		{
			TOptional<VeyraBackendProtocol::FParty> Read;
			FString Problem;
			const FString Members = Member(AccountId, TEXT("DevOne"), true, true) + TEXT(",") + Member(OtherAccountId, TEXT("DevTwo"), false, false);
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseParty(Party(TEXT("casual_select"), TEXT("queued"), Members, TEXT("12.5")), Read, Problem), Problem));
			ASSERT_THAT(IsTrue(Read.IsSet()));
			ASSERT_THAT(AreEqual(Read->Id, FString(PartyId)));
			ASSERT_THAT(IsTrue(Read->Status == VeyraBackendProtocol::EPartyStatus::Queued));
			ASSERT_THAT(IsTrue(Read->QueuedSeconds == 12.5));
			ASSERT_THAT(IsTrue(Read->Find(AccountId)->bLeader));
			ASSERT_THAT(IsFalse(Read->Find(OtherAccountId)->bReady));
			ASSERT_THAT(IsFalse(Read->AllReady()));

			// A party made by an invitation has no mode until its leader chooses one.
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseParty(Party(TEXT(""), TEXT("idle"), Member(AccountId, TEXT("DevOne"), false, true)), Read, Problem), Problem));
			ASSERT_THAT(IsTrue(Read->Mode.IsEmpty()));
			for (const TCHAR* Status : { TEXT("found"), TEXT("selecting") })
			{
				ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseParty(Party(TEXT("casual_select"), Status, Member(AccountId, TEXT("DevOne"), true, true)), Read, Problem), Problem));
			}
			ASSERT_THAT(IsTrue(Read->Status == VeyraBackendProtocol::EPartyStatus::Selecting));

			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseParty(TEXT("{\"party\":null}"), Read, Problem), Problem));
			ASSERT_THAT(IsFalse(Read.IsSet()));
		}

		TEST_METHOD(RefusesAPartyThatDoesNotFit)
		{
			const FString Leader = Member(AccountId, TEXT("DevOne"), true, true);
			const TArray<FString> Bodies = {
				TEXT("{}"),
				Party(TEXT("casual_select"), TEXT("waiting"), Leader),
				Party(TEXT("Casual Select"), TEXT("idle"), Leader),
				Party(TEXT("casual_select"), TEXT("idle"), Member(AccountId, TEXT("DevOne"), true, false)),
				Party(TEXT("casual_select"), TEXT("idle"), Leader + TEXT(",") + Member(OtherAccountId, TEXT("DevTwo"), true, true)),
				Party(TEXT("casual_select"), TEXT("idle"), Member(TEXT("no"), TEXT("DevOne"), true, true)),
				Party(TEXT("casual_select"), TEXT("queued"), Leader, TEXT("-1")),
			};
			for (const FString& Body : Bodies)
			{
				TOptional<VeyraBackendProtocol::FParty> Read;
				FString Problem;
				ASSERT_THAT(IsFalse(VeyraBackendProtocol::ParseParty(Body, Read, Problem), Body));
				ASSERT_THAT(IsFalse(Problem.IsEmpty()));
			}
		}

		TEST_METHOD(ReadsAMatchFound)
		{
			TOptional<VeyraBackendProtocol::FMatchFound> Read;
			FString Problem;
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseMatchFound(Found(TEXT("pending"), TEXT("accepted"), TEXT("1"), TEXT("2"), TEXT("null"), TEXT("null")), Read, Problem), Problem));
			ASSERT_THAT(IsTrue(Read.IsSet()));
			ASSERT_THAT(AreEqual(Read->Id, FString(FoundId)));
			ASSERT_THAT(AreEqual(Read->You, FString(TEXT("accepted"))));
			ASSERT_THAT(IsTrue(Read->Accepted == 1 && Read->Total == 2));
			ASSERT_THAT(IsTrue(Read->RemainingSeconds == 9.5));

			const FString Opened = FString::Printf(TEXT("\"%s\""), SelectId);
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseMatchFound(Found(TEXT("accepted"), TEXT("accepted"), TEXT("2"), TEXT("2"), *Opened, TEXT("null")), Read, Problem), Problem));
			ASSERT_THAT(AreEqual(Read->SelectId, FString(SelectId)));
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseMatchFound(Found(TEXT("abandoned"), TEXT("declined"), TEXT("0"), TEXT("2"), TEXT("null"), TEXT("\"declined\"")), Read, Problem), Problem));
			ASSERT_THAT(AreEqual(Read->AbandonReason, FString(TEXT("declined"))));

			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseMatchFound(TEXT("{\"matchFound\":null}"), Read, Problem), Problem));
			ASSERT_THAT(IsFalse(Read.IsSet()));

			for (const FString& Bad : { FString(TEXT("{}")), Found(TEXT("pending"), TEXT("pending"), TEXT("3"), TEXT("2"), TEXT("null"), TEXT("null")),
					 Found(TEXT("pending"), TEXT("pending"), TEXT("0"), TEXT("0"), TEXT("null"), TEXT("null")),
					 Found(TEXT("pending"), TEXT("Maybe!"), TEXT("0"), TEXT("2"), TEXT("null"), TEXT("null")),
					 Found(TEXT("accepted"), TEXT("accepted"), TEXT("2"), TEXT("2"), TEXT("\"../select\""), TEXT("null")) })
			{
				ASSERT_THAT(IsFalse(VeyraBackendProtocol::ParseMatchFound(Bad, Read, Problem), Bad));
				ASSERT_THAT(IsFalse(Problem.IsEmpty()));
			}
		}

		TEST_METHOD(WritesPartyChoices)
		{
			ASSERT_THAT(AreEqual(VeyraBackendProtocol::BuildModeBody(TEXT("casual_select")), FString(TEXT("{\"mode\":\"casual_select\"}"))));
			ASSERT_THAT(AreEqual(VeyraBackendProtocol::BuildReadyBody(true), FString(TEXT("{\"ready\":true}"))));
			ASSERT_THAT(AreEqual(VeyraBackendProtocol::BuildReadyBody(false), FString(TEXT("{\"ready\":false}"))));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
