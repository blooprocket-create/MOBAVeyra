// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER

#include "Backend/VeyraBackendProtocol.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformProcess.h"
#include "Handoff/VeyraPipeLineReader.h"
#include "Handoff/VeyraServerAssignment.h"
#include "Hash/VeyraSha256.h"
#include "Join/VeyraMatchHostSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "VeyraServicesSettings.h"

namespace VeyraServicesTests
{
	/** A value in a credential's format: its prefix and 43 base64url characters. It is not a credential. */
	FString ExampleCredential(const TCHAR* Prefix, TCHAR Fill)
	{
		return FString(Prefix) + FString::ChrN(43, Fill);
	}

	FString ExampleLaunchCode() { return ExampleCredential(TEXT("vlc_"), TEXT('A')); }
	FString ExampleGameSession() { return ExampleCredential(TEXT("vgs_"), TEXT('B')); }
	FString ExampleTicket() { return ExampleCredential(TEXT("vjt_"), TEXT('C')); }

	const TCHAR* const ExampleId = TEXT("11111111-2222-4333-8444-555555555555");

	/** Every problem in one line, for assertion messages. */
	FString Describe(const TArray<FString>& Problems)
	{
		return FString::Join(Problems, TEXT(" | "));
	}

	// Veyra.Services.Formats.*: the credential and address formats the backend defines (ADR-007 §2).
	TEST_CLASS(Formats, "Veyra.Services")
	{
		TEST_METHOD(AcceptsALaunchCodeInTheBackendsFormat)
		{
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::IsLaunchCode(ExampleLaunchCode())));
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::IsLaunchCode(TEXT("vlc_") + FString::ChrN(42, TEXT('a')) + TEXT("_"))));
		}

		TEST_METHOD(RefusesAnythingElseAsALaunchCode)
		{
			ASSERT_THAT(IsFalse(VeyraBackendProtocol::IsLaunchCode(ExampleGameSession())));
			ASSERT_THAT(IsFalse(VeyraBackendProtocol::IsLaunchCode(TEXT("vlc_") + FString::ChrN(42, TEXT('A')))));
			ASSERT_THAT(IsFalse(VeyraBackendProtocol::IsLaunchCode(TEXT("vlc_") + FString::ChrN(44, TEXT('A')))));
			ASSERT_THAT(IsFalse(VeyraBackendProtocol::IsLaunchCode(TEXT("vlc_") + FString::ChrN(42, TEXT('A')) + TEXT("+"))));
			ASSERT_THAT(IsFalse(VeyraBackendProtocol::IsLaunchCode(ExampleLaunchCode() + TEXT("\n"))));
			ASSERT_THAT(IsFalse(VeyraBackendProtocol::IsLaunchCode(TEXT(" ") + ExampleLaunchCode())));
			ASSERT_THAT(IsFalse(VeyraBackendProtocol::IsLaunchCode(TEXT(""))));
		}

		TEST_METHOD(AcceptsOnlyBuildVersionsTheBackendAccepts)
		{
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::IsBuildVersion(TEXT("0.1.0"))));
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::IsBuildVersion(TEXT("1.2.3+ci_4-rc"))));
			ASSERT_THAT(IsFalse(VeyraBackendProtocol::IsBuildVersion(TEXT(""))));
			ASSERT_THAT(IsFalse(VeyraBackendProtocol::IsBuildVersion(TEXT("1.0 beta"))));
			ASSERT_THAT(IsFalse(VeyraBackendProtocol::IsBuildVersion(FString::ChrN(65, TEXT('1')))));
		}

		TEST_METHOD(AcceptsOnlyABaseUrlWithNoPath)
		{
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::IsBaseUrl(TEXT("http://127.0.0.1:8080"))));
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::IsBaseUrl(TEXT("https://backend"))));
			ASSERT_THAT(IsFalse(VeyraBackendProtocol::IsBaseUrl(TEXT("http://backend/"))));
			ASSERT_THAT(IsFalse(VeyraBackendProtocol::IsBaseUrl(TEXT("http://backend:8080/v1"))));
			ASSERT_THAT(IsFalse(VeyraBackendProtocol::IsBaseUrl(TEXT("http://backend?x=1"))));
			ASSERT_THAT(IsFalse(VeyraBackendProtocol::IsBaseUrl(TEXT("ftp://backend"))));
			ASSERT_THAT(IsFalse(VeyraBackendProtocol::IsBaseUrl(TEXT(""))));
		}

		TEST_METHOD(RedactsEveryCredential)
		{
			const FString Text = FString::Printf(TEXT("session %s, ticket %s and a cut-off vms_ab"), *ExampleGameSession(), *ExampleTicket());
			ASSERT_THAT(AreEqual(VeyraBackendProtocol::RedactCredentials(Text),
				FString(TEXT("session vgs_<redacted>, ticket vjt_<redacted> and a cut-off vms_<redacted>"))));
			ASSERT_THAT(AreEqual(VeyraBackendProtocol::RedactCredentials(TEXT("no secrets here")), FString(TEXT("no secrets here"))));
		}
	};

	// Veyra.Services.GameSession.*: reading the answer to a redeemed launch code (ADR-005 L3).
	TEST_CLASS(GameSession, "Veyra.Services")
	{
		TEST_METHOD(ReadsTheSessionAndAccount)
		{
			const FString Body = FString::Printf(TEXT("{\"token\":\"%s\",\"expiresAt\":\"2026-09-26T12:00:00Z\",\"account\":{\"id\":\"%s\",\"displayName\":\"DevOne\"}}"),
				*ExampleGameSession(), ExampleId);
			VeyraBackendProtocol::FGameSession Session;
			FString Problem;
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseGameSession(Body, Session, Problem), Problem));
			ASSERT_THAT(AreEqual(Session.Token, ExampleGameSession()));
			ASSERT_THAT(AreEqual(Session.AccountId, FString(ExampleId)));
			ASSERT_THAT(AreEqual(Session.DisplayName, FString(TEXT("DevOne"))));
		}

		TEST_METHOD(RefusesAnswersThatAreNotASession)
		{
			const TArray<FString> Bodies = {
				TEXT("not json"),
				FString::Printf(TEXT("{\"token\":\"%s\"}"), *ExampleGameSession()),
				FString::Printf(TEXT("{\"token\":\"%s\",\"account\":{\"id\":\"%s\",\"displayName\":\"DevOne\"}}"), *ExampleLaunchCode(), ExampleId),
				FString::Printf(TEXT("{\"token\":\"%s\",\"account\":{\"id\":\"not-an-id\",\"displayName\":\"DevOne\"}}"), *ExampleGameSession()),
				FString::Printf(TEXT("{\"token\":\"%s\",\"account\":{\"id\":\"%s\",\"displayName\":7}}"), *ExampleGameSession(), ExampleId),
			};
			for (const FString& Body : Bodies)
			{
				VeyraBackendProtocol::FGameSession Session;
				FString Problem;
				ASSERT_THAT(IsFalse(VeyraBackendProtocol::ParseGameSession(Body, Session, Problem), Body));
				ASSERT_THAT(IsFalse(Problem.IsEmpty()));
				ASSERT_THAT(IsFalse(Problem.Contains(TEXT("vgs_")) || Problem.Contains(TEXT("vlc_")), TEXT("a problem quoted a credential")));
			}
		}
	};

	// Veyra.Services.MyMatch.*: reading the player's match (ADR-007 §10).
	TEST_CLASS(MyMatch, "Veyra.Services")
	{
		static FString ReadyBody(const FString& Host, const FString& Port, const FString& Ticket)
		{
			return FString::Printf(TEXT("{\"match\":{\"id\":\"%s\",\"state\":\"ready\",\"side\":\"A\",\"server\":{\"host\":\"%s\",\"port\":%s},\"ticket\":%s}}"),
				ExampleId, *Host, *Port, *Ticket);
		}

		TEST_METHOD(ReadsNoMatch)
		{
			VeyraBackendProtocol::FMyMatch Match;
			FString Problem;
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseMyMatch(TEXT("{\"match\":null}"), Match, Problem), Problem));
			ASSERT_THAT(IsFalse(Match.bHasMatch));
			ASSERT_THAT(IsFalse(Match.bReady));
		}

		TEST_METHOD(ReadsAMatchThatIsStarting)
		{
			const FString Body = FString::Printf(TEXT("{\"match\":{\"id\":\"%s\",\"state\":\"allocating\",\"side\":\"B\",\"server\":null,\"ticket\":null}}"), ExampleId);
			VeyraBackendProtocol::FMyMatch Match;
			FString Problem;
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseMyMatch(Body, Match, Problem), Problem));
			ASSERT_THAT(IsTrue(Match.bHasMatch));
			ASSERT_THAT(IsFalse(Match.bReady));
			ASSERT_THAT(AreEqual(Match.MatchId, FString(ExampleId)));
		}

		TEST_METHOD(ReadsAReadyMatch)
		{
			VeyraBackendProtocol::FMyMatch Match;
			FString Problem;
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseMyMatch(ReadyBody(TEXT("127.0.0.1"), TEXT("7780"), TEXT("\"") + ExampleTicket() + TEXT("\"")), Match, Problem), Problem));
			ASSERT_THAT(IsTrue(Match.bHasMatch && Match.bReady));
			ASSERT_THAT(AreEqual(Match.Host, FString(TEXT("127.0.0.1"))));
			ASSERT_THAT(AreEqual(Match.Port, 7780));
			ASSERT_THAT(AreEqual(Match.Ticket, ExampleTicket()));
		}

		TEST_METHOD(RefusesAMatchItCannotJoinSafely)
		{
			const FString Ticket = TEXT("\"") + ExampleTicket() + TEXT("\"");
			const TArray<FString> Bodies = {
				TEXT("[]"),
				TEXT("{}"),
				FString::Printf(TEXT("{\"match\":{\"id\":\"%s\",\"state\":\"ended\"}}"), ExampleId),
				FString(TEXT("{\"match\":{\"id\":\"no\",\"state\":\"allocating\"}}")),
				ReadyBody(TEXT("127.0.0.1"), TEXT("7780"), TEXT("null")),
				ReadyBody(TEXT("127.0.0.1?Name=x"), TEXT("7780"), Ticket),
				ReadyBody(TEXT("127.0.0.1"), TEXT("0"), Ticket),
				ReadyBody(TEXT("127.0.0.1"), TEXT("65536"), Ticket),
				ReadyBody(TEXT("127.0.0.1"), TEXT("7780.5"), Ticket),
				ReadyBody(TEXT("127.0.0.1"), TEXT("\"7780\""), Ticket),
				ReadyBody(TEXT("127.0.0.1"), TEXT("7780"), TEXT("\"") + ExampleGameSession() + TEXT("\"")),
			};
			for (const FString& Body : Bodies)
			{
				VeyraBackendProtocol::FMyMatch Match;
				FString Problem;
				ASSERT_THAT(IsFalse(VeyraBackendProtocol::ParseMyMatch(Body, Match, Problem), Body));
				ASSERT_THAT(IsFalse(Problem.IsEmpty()));
				ASSERT_THAT(IsFalse(Problem.Contains(TEXT("vjt_")) || Problem.Contains(TEXT("vgs_")), TEXT("a problem quoted a credential")));
			}
		}
	};

	// Veyra.Services.ResultBody.*: the result a match server reports (ADR-007 §7).
	TEST_CLASS(ResultBody, "Veyra.Services")
	{
		static TSharedPtr<FJsonObject> Parse(const FString& Body)
		{
			TSharedPtr<FJsonObject> Object;
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), Object);
			return Object;
		}

		TEST_METHOD(CarriesTheBackendsFieldsAndNames)
		{
			FVeyraMatchResult Result;
			Result.MatchId = ExampleId;
			Result.EndReason = EVeyraMatchEndReason::DeveloperRequest;
			Result.DurationSeconds = 12.5;
			Result.Participants = { { TEXT("a"), true, true }, { TEXT("b"), true, false } };
			const TSharedPtr<FJsonObject> Body = Parse(VeyraBackendProtocol::BuildResultBody(Result));
			ASSERT_THAT(IsTrue(Body.IsValid()));
			ASSERT_THAT(AreEqual(Body->Values.Num(), 4));
			ASSERT_THAT(AreEqual(Body->GetStringField(TEXT("endReason")), FString(TEXT("developer_request"))));
			ASSERT_THAT(IsTrue(Body->HasTypedField<EJson::Null>(TEXT("winner"))));
			ASSERT_THAT(IsTrue(Body->GetNumberField(TEXT("durationSeconds")) == 12.5));
			const TArray<TSharedPtr<FJsonValue>>& Participants = Body->GetArrayField(TEXT("participants"));
			ASSERT_THAT(AreEqual(Participants.Num(), 2));
			const TSharedPtr<FJsonObject>& Second = Participants[1]->AsObject();
			ASSERT_THAT(AreEqual(Second->Values.Num(), 3));
			ASSERT_THAT(AreEqual(Second->GetStringField(TEXT("accountId")), FString(TEXT("b"))));
			ASSERT_THAT(IsTrue(Second->GetBoolField(TEXT("joined"))));
			ASSERT_THAT(IsFalse(Second->GetBoolField(TEXT("connectedAtEnd"))));
		}

		TEST_METHOD(NamesAnAbandonedMatchAndAWinner)
		{
			FVeyraMatchResult Result;
			Result.EndReason = EVeyraMatchEndReason::Abandoned;
			Result.Winner = EVeyraTeam::B;
			const TSharedPtr<FJsonObject> Body = Parse(VeyraBackendProtocol::BuildResultBody(Result));
			ASSERT_THAT(IsTrue(Body.IsValid()));
			ASSERT_THAT(AreEqual(Body->GetStringField(TEXT("endReason")), FString(TEXT("abandoned"))));
			ASSERT_THAT(AreEqual(Body->GetStringField(TEXT("winner")), FString(TEXT("B"))));
			ASSERT_THAT(AreEqual(Body->GetArrayField(TEXT("participants")).Num(), 0));
		}

		TEST_METHOD(NamesAHostEndedMatch)
		{
			FVeyraMatchResult Result;
			Result.EndReason = EVeyraMatchEndReason::HostEnded;
			const TSharedPtr<FJsonObject> Body = Parse(VeyraBackendProtocol::BuildResultBody(Result));
			ASSERT_THAT(IsTrue(Body.IsValid()));
			ASSERT_THAT(AreEqual(Body->GetStringField(TEXT("endReason")), FString(TEXT("host_ended"))));
			ASSERT_THAT(IsTrue(Body->HasTypedField<EJson::Null>(TEXT("winner"))));
		}
	};

	// Veyra.Services.Settings.*: the backend's address and every wait are validated settings.
	TEST_CLASS(Settings, "Veyra.Services")
	{
		TEST_METHOD(TheProjectsSettingsAreValid)
		{
			const TArray<FString> Problems = GetDefault<UVeyraServicesSettings>()->Validate();
			ASSERT_THAT(IsTrue(Problems.IsEmpty(), Describe(Problems)));
		}

		TEST_METHOD(NamesEveryMissingSetting)
		{
			UVeyraServicesSettings* Candidate = NewObject<UVeyraServicesSettings>(GetTransientPackage());
			Candidate->BackendBaseUrl.Reset();
			Candidate->RequestTimeoutSeconds = 0.0f;
			Candidate->LaunchCodeReadTimeoutSeconds = 0.0f;
			Candidate->MatchPollIntervalSeconds = 0.0f;
			Candidate->MatchWaitTimeoutSeconds = -1.0f;
			Candidate->ClientRequestAttempts = 0;
			Candidate->ClientRetryIntervalSeconds = 0.0f;
			Candidate->SelectPollIntervalSeconds = 0.0f;
			Candidate->ResultPollIntervalSeconds = 0.0f;
			Candidate->ResultWaitTimeoutSeconds = 0.0f;
			Candidate->ReconnectPollIntervalSeconds = 0.0f;
			Candidate->AssignmentReadTimeoutSeconds = 0.0f;
			Candidate->AssignmentPollIntervalSeconds = 0.0f;
			Candidate->ReportAttempts = 0;
			Candidate->ReportRetryIntervalSeconds = 0.0f;
			const FString Problems = Describe(Candidate->Validate());
			for (const TCHAR* Name : { TEXT("BackendBaseUrl"), TEXT("RequestTimeoutSeconds"), TEXT("LaunchCodeReadTimeoutSeconds"),
					 TEXT("MatchPollIntervalSeconds"), TEXT("MatchWaitTimeoutSeconds"), TEXT("ClientRequestAttempts"), TEXT("ClientRetryIntervalSeconds"),
					 TEXT("SelectPollIntervalSeconds"), TEXT("ResultPollIntervalSeconds"), TEXT("ResultWaitTimeoutSeconds"),
					 TEXT("ReconnectPollIntervalSeconds"), TEXT("AssignmentReadTimeoutSeconds"), TEXT("AssignmentPollIntervalSeconds"),
					 TEXT("ReportAttempts"), TEXT("ReportRetryIntervalSeconds") })
			{
				ASSERT_THAT(IsTrue(Problems.Contains(Name), FString::Printf(TEXT("%s is not named in: %s"), Name, *Problems)));
			}
		}
	};

	// Veyra.Services.PipeReader.*: lines from a pipe, the way a launch code or an assignment arrives.
	TEST_CLASS(PipeReader, "Veyra.Services")
	{
		void* ReadEnd = nullptr;
		void* WriteEnd = nullptr;
		TUniquePtr<FVeyraPipeLineReader> Reader;

		/** Bytes per write in the long-line test: less than any pipe buffer, so a write never blocks. */
		static constexpr int32 ChunkBytes = 1024;

		BEFORE_EACH()
		{
			ASSERT_THAT(IsTrue(FPlatformProcess::CreatePipe(ReadEnd, WriteEnd)));
#if PLATFORM_WINDOWS
			Reader = MakeUnique<FVeyraPipeLineReader>(ReadEnd);
#else
			Reader = MakeUnique<FVeyraPipeLineReader>(static_cast<FPipeHandle*>(ReadEnd)->GetHandle());
#endif
		}

		AFTER_EACH()
		{
			Reader.Reset();
			FPlatformProcess::ClosePipe(ReadEnd, WriteEnd);
		}

		void WriteText(const char* Text)
		{
			const int32 Length = FCStringAnsi::Strlen(Text);
			ASSERT_THAT(IsTrue(FPlatformProcess::WritePipe(WriteEnd, reinterpret_cast<const uint8*>(Text), Length)));
		}

		void CloseWriteEnd()
		{
			FPlatformProcess::ClosePipe(nullptr, WriteEnd);
			WriteEnd = nullptr;
		}

		EVeyraPipeRead PollLine(FString& Line)
		{
			Line.Reset();
			return Reader->Poll(Line);
		}

		TEST_METHOD(ReadsALine)
		{
			FString Line;
			ASSERT_THAT(IsTrue(PollLine(Line) == EVeyraPipeRead::Pending));
			WriteText("hello\n");
			ASSERT_THAT(IsTrue(PollLine(Line) == EVeyraPipeRead::Line));
			ASSERT_THAT(AreEqual(Line, FString(TEXT("hello"))));
			ASSERT_THAT(IsTrue(PollLine(Line) == EVeyraPipeRead::Pending));
		}

		TEST_METHOD(JoinsPartialWritesAndKeepsAFinalLine)
		{
			FString Line;
			WriteText("hel");
			ASSERT_THAT(IsTrue(PollLine(Line) == EVeyraPipeRead::Pending));
			WriteText("lo\r\nlast");
			ASSERT_THAT(IsTrue(PollLine(Line) == EVeyraPipeRead::Line));
			ASSERT_THAT(AreEqual(Line, FString(TEXT("hello"))));
			ASSERT_THAT(IsTrue(PollLine(Line) == EVeyraPipeRead::Pending));
			CloseWriteEnd();
			ASSERT_THAT(IsTrue(PollLine(Line) == EVeyraPipeRead::Line));
			ASSERT_THAT(AreEqual(Line, FString(TEXT("last"))));
			ASSERT_THAT(IsTrue(PollLine(Line) == EVeyraPipeRead::EndOfInput));
			ASSERT_THAT(IsTrue(PollLine(Line) == EVeyraPipeRead::EndOfInput));
		}

		TEST_METHOD(EndsWhenTheWriterClosesWithoutALine)
		{
			FString Line;
			CloseWriteEnd();
			ASSERT_THAT(IsTrue(PollLine(Line) == EVeyraPipeRead::EndOfInput));
		}

		TEST_METHOD(DecodesUtf8)
		{
			FString Line;
			WriteText("\xC3\xA9\n");
			ASSERT_THAT(IsTrue(PollLine(Line) == EVeyraPipeRead::Line));
			ASSERT_THAT(AreEqual(Line, FString(TEXT("é"))));
		}

		TEST_METHOD(RefusesALineOverTheLimit)
		{
			TArray<char> Chunk;
			Chunk.Init('x', ChunkBytes + 1);
			Chunk.Last() = '\0';
			FString Line;
			EVeyraPipeRead Read = EVeyraPipeRead::Pending;
			for (int32 Written = 0; Read == EVeyraPipeRead::Pending && Written <= FVeyraPipeLineReader::MaxLineBytes + ChunkBytes; Written += ChunkBytes)
			{
				WriteText(Chunk.GetData());
				Read = PollLine(Line);
			}
			ASSERT_THAT(IsTrue(Read == EVeyraPipeRead::TooLong));
			WriteText("\n");
			ASSERT_THAT(IsTrue(PollLine(Line) == EVeyraPipeRead::TooLong));
		}

		TEST_METHOD(RefusesAHandleThatIsNotAPipe)
		{
#if PLATFORM_WINDOWS
			FVeyraPipeLineReader Nothing(nullptr);
#else
			FVeyraPipeLineReader Nothing(-1);
#endif
			FString Line;
			ASSERT_THAT(IsTrue(Nothing.Poll(Line) == EVeyraPipeRead::NotAPipe));
		}
	};

	// Veyra.Services.Assignment.*: a match server's assignment (ADR-007 §5).
	TEST_CLASS(Assignment, "Veyra.Services")
	{
#if WITH_EDITOR
		// These read files from the project folder, which exists only in editor builds.
		FString SchemaText;
		FString Example;

		BEFORE_EACH()
		{
			const TArray<FString> Problems = VeyraServerAssignment::ReadSchema(SchemaText);
			ASSERT_THAT(IsTrue(Problems.IsEmpty(), Describe(Problems)));
			// Written by the backend's contract test (Backend/internal/match/contract_test.go).
			const FString ExamplePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/VeyraDeveloper/TestData/MatchAssignment.example.json"));
			ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(Example, *ExamplePath), ExamplePath));
			// The reader gives the line without its newline.
			Example.TrimEndInline();
		}

		TArray<FString> ProblemsWith(const FString& From, const FString& To) const
		{
			FVeyraServerAssignment Out;
			return VeyraServerAssignment::Parse(Example.Replace(*From, *To, ESearchCase::CaseSensitive), SchemaText, Out);
		}

		TEST_METHOD(ReadsTheBackendsExample)
		{
			FVeyraServerAssignment Parsed;
			const TArray<FString> Problems = VeyraServerAssignment::Parse(Example, SchemaText, Parsed);
			ASSERT_THAT(IsTrue(Problems.IsEmpty(), Describe(Problems)));
			ASSERT_THAT(AreEqual(Parsed.Match.MatchId, FString(ExampleId)));
			ASSERT_THAT(AreEqual(Parsed.BackendUrl, FString(TEXT("http://backend:8080"))));
			ASSERT_THAT(IsTrue(Parsed.ServerCredential.StartsWith(TEXT("vms_"), ESearchCase::CaseSensitive)));
			ASSERT_THAT(AreEqual(Parsed.Match.Mode.ToString(), FString(TEXT("casual_select"))));
			ASSERT_THAT(IsTrue(Parsed.Match.Rules == EVeyraMatchRules::Standard && Parsed.Match.HostAccountId.IsEmpty()));
			ASSERT_THAT(AreEqual(Parsed.Match.Participants.Num(), 2));
			const FVeyraAssignedParticipant& First = Parsed.Match.Participants[0];
			ASSERT_THAT(AreEqual(First.DisplayName, FString(TEXT("DevOne"))));
			ASSERT_THAT(IsTrue(First.Side == EVeyraTeam::A));
			ASSERT_THAT(AreEqual(First.VanguardId.ToString(), FString(TEXT("cairn"))));
			ASSERT_THAT(IsTrue(Parsed.Match.Participants[1].Side == EVeyraTeam::B));
			ASSERT_THAT(AreEqual(Parsed.Match.Participants[1].VanguardId.ToString(), FString(TEXT("oriel"))));
			// The backend's ticket vector (Backend/internal/match/ticket_test.go): the game hashes the
			// ticket the backend derived for this participant to the hash the backend sent.
			ASSERT_THAT(AreEqual(First.TicketHash, VeyraHash::Sha256Hex(TEXT("vjt_xdMWyGQJg9xC_-yn9b-5ZYoh8_KKDRs9Bfjlh7WgwKQ"))));
		}

		TEST_METHOD(TheExampleIsAMatchThisBuildCanHost)
		{
			FVeyraServerAssignment Parsed;
			ASSERT_THAT(IsTrue(VeyraServerAssignment::Parse(Example, SchemaText, Parsed).IsEmpty()));
			UVeyraMatchHostSubsystem* Host = UVeyraMatchHostSubsystem::Get();
			const TArray<FString> Problems = Host->SetAssignment(Parsed.Match);
			Host->ClearAssignment();
			ASSERT_THAT(IsTrue(Problems.IsEmpty(), Describe(Problems)));
		}

		TEST_METHOD(ReadsAPracticeAssignment)
		{
			const FString Practice = Example.Replace(TEXT("\"rules\":\"Standard\",\"hostAccountId\":[]"),
				TEXT("\"rules\":\"Practice\",\"hostAccountId\":[\"aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee\"]"), ESearchCase::CaseSensitive);
			ASSERT_THAT(IsTrue(Practice != Example));
			FVeyraServerAssignment Parsed;
			const TArray<FString> Problems = VeyraServerAssignment::Parse(Practice, SchemaText, Parsed);
			ASSERT_THAT(IsTrue(Problems.IsEmpty(), Describe(Problems)));
			ASSERT_THAT(IsTrue(Parsed.Match.Rules == EVeyraMatchRules::Practice));
			ASSERT_THAT(AreEqual(Parsed.Match.HostAccountId, FString(TEXT("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"))));
		}

		TEST_METHOD(RefusesABrokenAssignment)
		{
			const TArray<TPair<const TCHAR*, const TCHAR*>> Breaks = {
				// A version 1 assignment carries no Vanguards; this build reads version 2 only.
				{ TEXT("\"schemaVersion\":2"), TEXT("\"schemaVersion\":1") },
				{ TEXT("\"rules\":\"Standard\""), TEXT("\"rules\":\"Draft\"") },
				{ TEXT("\"mode\":\"casual_select\""), TEXT("\"mode\":\"Casual Select\"") },
				{ TEXT("\"hostAccountId\":[]"), TEXT("\"hostAccountId\":[\"a\",\"b\"]") },
				{ TEXT("\"vanguardId\":\"cairn\""), TEXT("\"vanguardId\":\"Cairn\"") },
				{ TEXT("\"side\":\"B\""), TEXT("\"side\":\"C\"") },
				{ TEXT("\"ticketHash\":\"f9de"), TEXT("\"ticketHash\":\"F9DE") },
				{ TEXT("\"serverCredential\":\"vms_"), TEXT("\"serverCredential\":\"vgs_") },
				{ TEXT("\"backendUrl\":\"http://backend:8080\""), TEXT("\"backendUrl\":\"http://backend:8080/v1\"") },
				{ TEXT("\"displayName\":\"DevOne\""), TEXT("\"displayName\":\"Dev\\u0007One\"") },
				{ TEXT("\"matchId\""), TEXT("\"extra\":1,\"matchId\"") },
			};
			for (const TPair<const TCHAR*, const TCHAR*>& Break : Breaks)
			{
				ASSERT_THAT(IsTrue(Example.Contains(Break.Key, ESearchCase::CaseSensitive), Break.Key));
				ASSERT_THAT(IsFalse(ProblemsWith(Break.Key, Break.Value).IsEmpty(), Break.Value));
			}
			FVeyraServerAssignment Out;
			const int32 RosterStart = Example.Find(TEXT("\"participants\":["), ESearchCase::CaseSensitive);
			const FString NoOne = Example.Left(RosterStart) + TEXT("\"participants\":[]}");
			ASSERT_THAT(IsFalse(VeyraServerAssignment::Parse(NoOne, SchemaText, Out).IsEmpty()));
		}

		TEST_METHOD(ProblemsNeverShowTheCredential)
		{
			// A JSON syntax error quotes the text around it, which is the whole line.
			// The credential is the field before the mode.
			const int32 CredentialEnd = Example.Find(TEXT("\",\"mode\""), ESearchCase::CaseSensitive);
			ASSERT_THAT(IsTrue(CredentialEnd > 0));
			FVeyraServerAssignment Out;
			const TArray<FString> Problems = VeyraServerAssignment::Parse(Example.Left(CredentialEnd) + TEXT("\"}}"), SchemaText, Out);
			ASSERT_THAT(IsFalse(Problems.IsEmpty()));
			ASSERT_THAT(IsFalse(Describe(Problems).Contains(TEXT("EXAMPLE-ONLY")), Describe(Problems)));
		}
#endif // WITH_EDITOR
	};
}

#endif // WITH_AUTOMATION_WORKER
