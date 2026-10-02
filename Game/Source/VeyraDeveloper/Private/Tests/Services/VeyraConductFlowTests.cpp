// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER

#include "Backend/VeyraConductProtocol.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Tests/Services/VeyraClientFlowTestRig.h"

namespace VeyraClientFlowTests
{
	// Fixture answers, independent of the committed backend configuration.
	inline FString ConductAnswer(const TCHAR* ReportedJson = TEXT("[]"), const TCHAR* CommendedJson = TEXT("null"))
	{
		return FString::Printf(TEXT("{\"conduct\":{\"reported\":%s,\"commended\":%s,\"players\":[{\"name\":\"DevTwo\",\"teammate\":false},{\"name\":\"DevThree\",\"teammate\":true}],")
							   TEXT("\"reasons\":[\"abusive_chat\",\"afk\",\"other\"],\"detailsMaxCharacters\":500}}"),
			ReportedJson, CommendedJson);
	}

	inline FString ConductRoute(const TCHAR* Match, const TCHAR* Leaf)
	{
		return FString::Printf(TEXT("/v1/me/matches/%s/%s"), Match, Leaf);
	}

	inline FString ConductBodyField(const FString& Body, const TCHAR* Field)
	{
		TSharedPtr<FJsonObject> Root;
		FString Value;
		return FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(Body), Root) && Root->TryGetStringField(Field, Value) ? Value : FString();
	}

	inline const TCHAR* const ReportReceived = TEXT("{\"report\":{\"reportedName\":\"DevTwo\",\"status\":\"received\"}}");

	// Veyra.Services.ConductFlow.*: the player's own record of a match, reports and commendation (ADR-047),
	// driven through the fake backend as the results screen and Match History drive them.
	TEST_CLASS(ConductFlow, "Veyra.Services")
	{
		FClientFlowTestRig Rig;
		FFlowTestBackend& Backend = Rig.Backend;

		const FVeyraClientSnapshot& Snapshot() const { return Rig.Flow->GetSnapshot(); }

		/** The results of the scored match, with the player's record read: nobody reported or commended yet. */
		bool ReachResultsWithConduct()
		{
			return Rig.ReachResults(ScoredOutcomeBody()) && Backend.Answer(TEXT("GET"), ConductRoute(MatchId, TEXT("conduct")), 200, ConductAnswer())
				&& Snapshot().Conduct.bLoaded;
		}

		TEST_METHOD(TheResultsReadThePlayersOwnRecord)
		{
			ASSERT_THAT(IsTrue(Rig.ReachResults(ScoredOutcomeBody())));
			ASSERT_THAT(IsFalse(Rig.Flow->ReportPlayer(TEXT("DevTwo"), TEXT("afk"), FString()), TEXT("nothing before the record is read")));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), ConductRoute(MatchId, TEXT("conduct")), 200, ConductAnswer(TEXT("[\"DevTwo\"]"), TEXT("\"DevThree\"")))));
			const FVeyraConduct& Conduct = Snapshot().Conduct;
			ASSERT_THAT(IsTrue(Conduct.bLoaded && Conduct.MatchId == MatchId && Conduct.Record.Reported == TArray<FString>{ TEXT("DevTwo") }
				&& Conduct.Record.Commended == TEXT("DevThree")));
			ASSERT_THAT(IsTrue(Conduct.Record.Reasons.Num() == 3 && Conduct.Record.DetailsMaxCharacters == 500));
			ASSERT_THAT(IsTrue(Conduct.Record.Players.Num() == 2 && Conduct.Record.Players[0].Name == TEXT("DevTwo") && !Conduct.Record.Players[0].bTeammate
				&& Conduct.Record.Players[1].bTeammate, TEXT("the other humans, never a bot")));
			// Reported and commended already: nothing more to send.
			ASSERT_THAT(IsFalse(Rig.Flow->ReportPlayer(TEXT("DevTwo"), TEXT("afk"), FString())));
			ASSERT_THAT(IsFalse(Rig.Flow->CommendTeammate(TEXT("DevTwo"))));
		}

		TEST_METHOD(AReportSaysOnlyThatItWasReceived)
		{
			ASSERT_THAT(IsTrue(ReachResultsWithConduct()));
			ASSERT_THAT(IsFalse(Rig.Flow->ReportPlayer(TEXT("DevTwo"), TEXT("speeding"), FString()), TEXT("a reason the record does not offer")));
			ASSERT_THAT(IsFalse(Rig.Flow->ReportPlayer(TEXT("DevOne"), TEXT("afk"), FString()), TEXT("never oneself")));
			ASSERT_THAT(IsTrue(Rig.Flow->ReportPlayer(TEXT("DevTwo"), TEXT("afk"), TEXT("stood in base"))));
			const FString ReportsPath = ConductRoute(MatchId, TEXT("reports"));
			const FFlowTestBackend::FRequest* Request = Backend.Find(TEXT("POST"), ReportsPath);
			ASSERT_THAT(IsNotNull(Request));
			ASSERT_THAT(IsTrue(ConductBodyField(Request->Body, TEXT("reportedName")) == TEXT("DevTwo") && ConductBodyField(Request->Body, TEXT("reason")) == TEXT("afk")
				&& ConductBodyField(Request->Body, TEXT("details")) == TEXT("stood in base") && ConductBodyField(Request->Body, TEXT("clientId")).Len() == 36));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), ReportsPath, 200, ReportReceived)));
			ASSERT_THAT(IsTrue(Snapshot().Conduct.Feedback == TEXT("report_sent") && Snapshot().Conduct.FeedbackName == TEXT("DevTwo")));
			ASSERT_THAT(IsTrue(Snapshot().Conduct.Record.Reported.Contains(TEXT("DevTwo"))));
			ASSERT_THAT(IsFalse(Rig.Flow->ReportPlayer(TEXT("DevTwo"), TEXT("other"), FString()), TEXT("one report per player and match")));
		}

		TEST_METHOD(ALostReportKeepsItsIdWhenSentAgain)
		{
			TestRunner->AddExpectedMessagePlain(TEXT("VeyraClientFlow: problem in Results (backend_unreachable)"), ELogVerbosity::Warning,
				EAutomationExpectedMessageFlags::Contains, 1);
			ASSERT_THAT(IsTrue(ReachResultsWithConduct()));
			const FString ReportsPath = ConductRoute(MatchId, TEXT("reports"));
			ASSERT_THAT(IsTrue(Rig.Flow->ReportPlayer(TEXT("DevTwo"), TEXT("afk"), FString())));
			const FString FirstId = ConductBodyField(Backend.Find(TEXT("POST"), ReportsPath)->Body, TEXT("clientId"));
			// No answer twice: the flow gives up and shows the problem.
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), ReportsPath, 0)));
			Rig.Advance(1.0);
			ASSERT_THAT(AreEqual(ConductBodyField(Backend.Find(TEXT("POST"), ReportsPath)->Body, TEXT("clientId")), FirstId, TEXT("its own retry")));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), ReportsPath, 0)));
			ASSERT_THAT(IsTrue(Snapshot().Problem.IsSet()));
			// Reporting the same player again is the same report, which the backend never files twice.
			ASSERT_THAT(IsTrue(Rig.Flow->ReportPlayer(TEXT("DevTwo"), TEXT("afk"), FString())));
			ASSERT_THAT(AreEqual(ConductBodyField(Backend.Find(TEXT("POST"), ReportsPath)->Body, TEXT("clientId")), FirstId));
		}

		TEST_METHOD(ACommendationGoesOnceAndARefusalShowsBesideThePlayer)
		{
			ASSERT_THAT(IsTrue(ReachResultsWithConduct()));
			const FString CommendPath = ConductRoute(MatchId, TEXT("commendation"));
			ASSERT_THAT(IsTrue(Rig.Flow->CommendTeammate(TEXT("DevTwo"))));
			ASSERT_THAT(AreEqual(ConductBodyField(Backend.Find(TEXT("POST"), CommendPath)->Body, TEXT("name")), FString(TEXT("DevTwo"))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), CommendPath, 409, ErrorBody(TEXT("not_teammate")))));
			ASSERT_THAT(IsTrue(Snapshot().Conduct.Feedback == TEXT("not_teammate") && Snapshot().Conduct.Record.Commended.IsEmpty() && !Snapshot().Problem.IsSet()));
			ASSERT_THAT(IsTrue(Rig.Flow->CommendTeammate(TEXT("DevThree"))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), CommendPath, 200, TEXT("{\"commendation\":{\"name\":\"DevThree\"}}"))));
			ASSERT_THAT(IsTrue(Snapshot().Conduct.Feedback == TEXT("commended") && Snapshot().Conduct.Record.Commended == TEXT("DevThree")));
			ASSERT_THAT(IsFalse(Rig.Flow->CommendTeammate(TEXT("DevFour")), TEXT("one per match")));
		}

		TEST_METHOD(AMatchHistoryRecordReportsButNeverCommends)
		{
			ASSERT_THAT(IsTrue(Rig.ReachShell()));
			ASSERT_THAT(IsTrue(Rig.Flow->LoadHistory({})));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/matches"), 200, HistoryBody({ HistoryEntry(MatchId, TEXT("win")) }, TEXT("null")))));
			ASSERT_THAT(IsTrue(Rig.Flow->OpenHistoryMatch(MatchId)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), MatchOutcomePath(), 200, ScoredOutcomeBody())));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), ConductRoute(MatchId, TEXT("conduct")), 200, ConductAnswer())));
			ASSERT_THAT(IsTrue(Snapshot().Conduct.bLoaded && Snapshot().Conduct.MatchId == MatchId));
			ASSERT_THAT(IsFalse(Rig.Flow->CommendTeammate(TEXT("DevTwo")), TEXT("only on the results screen")));
			ASSERT_THAT(IsTrue(Rig.Flow->ReportPlayer(TEXT("DevTwo"), TEXT("other"), FString())));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), ConductRoute(MatchId, TEXT("reports")), 200, ReportReceived)));
			ASSERT_THAT(IsTrue(Snapshot().Conduct.Feedback == TEXT("report_sent")));
			// Closing the record closes its conduct too.
			ASSERT_THAT(IsTrue(Rig.Flow->CloseHistoryMatch() && Snapshot().Conduct.MatchId.IsEmpty() && !Snapshot().Conduct.bLoaded));
		}

		TEST_METHOD(TheResultsScreenAlsoAddsFriendsAndInvitesToTheParty)
		{
			for (const EVeyraClientIntent Intent : { EVeyraClientIntent::SendFriendRequest, EVeyraClientIntent::InviteToParty, EVeyraClientIntent::ReportPlayer,
					 EVeyraClientIntent::CommendTeammate })
			{
				ASSERT_THAT(IsTrue(FVeyraClientFlow::IsIntentAllowed(EVeyraClientState::Results, Intent), LexToString(Intent)));
				for (const EVeyraClientState Elsewhere : { EVeyraClientState::MatchFound, EVeyraClientState::Selecting, EVeyraClientState::InMatch,
						 EVeyraClientState::ReconnectOnly })
				{
					ASSERT_THAT(IsFalse(FVeyraClientFlow::IsIntentAllowed(Elsewhere, Intent), LexToString(Intent)));
				}
			}
			ASSERT_THAT(IsTrue(FVeyraClientFlow::IsIntentAllowed(EVeyraClientState::Shell, EVeyraClientIntent::ReportPlayer), TEXT("from a Match History record")));
			ASSERT_THAT(IsFalse(FVeyraClientFlow::IsIntentAllowed(EVeyraClientState::Shell, EVeyraClientIntent::CommendTeammate)));
			// A friend request from the results goes by the name the scoreboard shows.
			ASSERT_THAT(IsTrue(ReachResultsWithConduct()));
			ASSERT_THAT(IsTrue(Rig.Flow->SendFriendRequest(TEXT("DevTwo"))));
			ASSERT_THAT(IsNotNull(Backend.Find(TEXT("GET"), VeyraBackendProtocol::AccountLookupPath(TEXT("DevTwo")))));
		}
	};

	// Veyra.Services.ConductProtocol.*: the conduct record as the client reads it, and the report and commendation
	// bodies as the backend reads them (ADR-047 §2–§4).
	TEST_CLASS(ConductProtocol, "Veyra.Services")
	{
		TEST_METHOD(ARecordIsReadAndAMalformedOneRefused)
		{
			VeyraBackendProtocol::FConductRecord Record;
			FString Problem;
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseConductRecord(ConductAnswer(TEXT("[\"DevTwo\"]")), Record, Problem), Problem));
			ASSERT_THAT(IsTrue(Record.Reported.Num() == 1 && Record.Commended.IsEmpty() && Record.Reasons[1] == TEXT("afk") && Record.DetailsMaxCharacters == 500));
			for (const TCHAR* Bad : {
					 TEXT("{\"conduct\":{\"players\":[],\"reported\":[],\"commended\":null,\"reasons\":[],\"detailsMaxCharacters\":500}}"),
					 TEXT("{\"conduct\":{\"players\":[],\"reported\":[],\"commended\":null,\"reasons\":[\"Not A Reason\"],\"detailsMaxCharacters\":500}}"),
					 TEXT("{\"conduct\":{\"players\":[],\"reported\":[],\"commended\":null,\"reasons\":[\"afk\"],\"detailsMaxCharacters\":-1}}"),
					 TEXT("{\"conduct\":{\"players\":[],\"reported\":[\"\"],\"commended\":null,\"reasons\":[\"afk\"],\"detailsMaxCharacters\":5}}"),
					 TEXT("{\"conduct\":{\"players\":[],\"reported\":[],\"reasons\":[\"afk\"],\"detailsMaxCharacters\":5}}"),
					 TEXT("{\"conduct\":{\"players\":[{\"name\":\"DevTwo\"}],\"reported\":[],\"commended\":null,\"reasons\":[\"afk\"],\"detailsMaxCharacters\":5}}"),
					 TEXT("{}") })
			{
				ASSERT_THAT(IsFalse(VeyraBackendProtocol::ParseConductRecord(Bad, Record, Problem), Bad));
			}
		}

		TEST_METHOD(AReportAndACommendationAreWrittenAsTheBackendReadsThem)
		{
			const FString Report = VeyraBackendProtocol::BuildReportBody(TEXT("DevTwo"), TEXT("abusive_chat"), TEXT("said \"x\""), TEXT("client-1234"));
			ASSERT_THAT(IsTrue(ConductBodyField(Report, TEXT("reportedName")) == TEXT("DevTwo") && ConductBodyField(Report, TEXT("reason")) == TEXT("abusive_chat")
				&& ConductBodyField(Report, TEXT("details")) == TEXT("said \"x\"") && ConductBodyField(Report, TEXT("clientId")) == TEXT("client-1234")));
			ASSERT_THAT(AreEqual(ConductBodyField(VeyraBackendProtocol::BuildCommendationBody(TEXT("DevThree")), TEXT("name")), FString(TEXT("DevThree"))));
		}
	};
}

#endif
