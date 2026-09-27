// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Backend/VeyraBackendProtocol.h"

#include "Dom/JsonObject.h"
#include "Internationalization/Regex.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace VeyraBackendProtocol
{
namespace
{
	// Formats the backend defines (Backend/internal/secret, identity and match). Every credential is
	// a prefix and the unpadded base64url form of 32 bytes, which is 43 characters.
	const TCHAR* const BaseUrlPattern = TEXT("^https?://[A-Za-z0-9.-]+(:[0-9]{1,5})?$");
	const TCHAR* const LaunchCodePattern = TEXT("^vlc_[A-Za-z0-9_-]{43}$");
	const TCHAR* const GameSessionPattern = TEXT("^vgs_[A-Za-z0-9_-]{43}$");
	const TCHAR* const JoinTicketPattern = TEXT("^vjt_[A-Za-z0-9_-]{43}$");
	const TCHAR* const BuildVersionPattern = TEXT("^[A-Za-z0-9._+-]{1,64}$");
	const TCHAR* const IdPattern = TEXT("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$");
	/** A host name or IPv4 address, and nothing that could add options to a travel URL. */
	const TCHAR* const HostPattern = TEXT("^[A-Za-z0-9.-]+$");
	/** The code in a backend error body, such as invalid_credentials. */
	const TCHAR* const ErrorCodePattern = TEXT("^[a-z_]{1,64}$");
	/** Any Veyra credential, including a truncated one. The first group is its prefix. */
	const TCHAR* const CredentialPattern = TEXT("(vls|vgs|vlc|vms|vjt)_[A-Za-z0-9_-]*");

	/** The highest TCP or UDP port. */
	constexpr int32 MaxPort = 65535;

	/** True if the whole of Text matches Pattern. ICU lets "$" match before a final newline, so the match must end at the text's end. */
	bool MatchesWhole(const TCHAR* Pattern, FStringView Text)
	{
		const FString Subject(Text);
		FRegexMatcher Matcher(FRegexPattern(FString(Pattern)), Subject);
		return Matcher.FindNext() && Matcher.GetMatchBeginning() == 0 && Matcher.GetMatchEnding() == Subject.Len();
	}

	TSharedPtr<FJsonObject> ParseObject(const FString& Body)
	{
		TSharedPtr<FJsonObject> Object;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
		if (!FJsonSerializer::Deserialize(Reader, Object))
		{
			return nullptr;
		}
		return Object;
	}

	/** A string field. FJsonObject would also turn numbers and booleans into strings. */
	bool StringField(const FJsonObject& Object, FStringView Name, FString& Out)
	{
		return Object.HasTypedField<EJson::String>(Name) && Object.TryGetStringField(Name, Out);
	}

	const TCHAR* EndReasonName(EVeyraMatchEndReason Reason)
	{
		switch (Reason)
		{
		case EVeyraMatchEndReason::DeveloperRequest:
			return TEXT("developer_request");
		case EVeyraMatchEndReason::Abandoned:
			return TEXT("abandoned");
		case EVeyraMatchEndReason::HostEnded:
			return TEXT("host_ended");
		}
		checkNoEntry();
		return TEXT("");
	}
}

bool IsBaseUrl(FStringView Text)
{
	return MatchesWhole(BaseUrlPattern, Text);
}

bool IsLaunchCode(FStringView Text)
{
	return MatchesWhole(LaunchCodePattern, Text);
}

bool IsBuildVersion(FStringView Text)
{
	return MatchesWhole(BuildVersionPattern, Text);
}

FString RedactCredentials(FStringView Text)
{
	const FString Subject(Text);
	FRegexMatcher Matcher(FRegexPattern(FString(CredentialPattern)), Subject);
	FString Redacted;
	int32 Copied = 0;
	while (Matcher.FindNext())
	{
		Redacted += Subject.Mid(Copied, Matcher.GetMatchBeginning() - Copied);
		Redacted += Matcher.GetCaptureGroup(1) + TEXT("_<redacted>");
		Copied = Matcher.GetMatchEnding();
	}
	Redacted += Subject.Mid(Copied);
	return Redacted;
}

FString BuildRedeemBody(const FString& LaunchCode, const FString& BuildVersion)
{
	FString Body;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("launchCode"), LaunchCode);
	Writer->WriteValue(TEXT("buildVersion"), BuildVersion);
	Writer->WriteObjectEnd();
	Writer->Close();
	return Body;
}

bool ParseGameSession(const FString& Body, FGameSession& Out, FString& OutProblem)
{
	const TSharedPtr<FJsonObject> Root = ParseObject(Body);
	const TSharedPtr<FJsonObject>* Account = nullptr;
	FGameSession Session;
	if (!Root.IsValid() || !StringField(*Root, TEXT("token"), Session.Token) || !Root->TryGetObjectField(TEXT("account"), Account)
		|| !StringField(**Account, TEXT("id"), Session.AccountId) || !StringField(**Account, TEXT("displayName"), Session.DisplayName))
	{
		OutProblem = TEXT("the answer is not a game session with a token and an account");
		return false;
	}
	if (!MatchesWhole(GameSessionPattern, Session.Token) || !MatchesWhole(IdPattern, Session.AccountId) || Session.DisplayName.IsEmpty())
	{
		OutProblem = TEXT("the game session's token, account ID or display name is not in the expected format");
		return false;
	}
	Out = MoveTemp(Session);
	return true;
}

bool ParseMyMatch(const FString& Body, FMyMatch& Out, FString& OutProblem)
{
	const TSharedPtr<FJsonObject> Root = ParseObject(Body);
	if (!Root.IsValid() || !Root->HasField(TEXT("match")))
	{
		OutProblem = TEXT("the answer has no \"match\"");
		return false;
	}
	FMyMatch Match;
	if (Root->HasTypedField<EJson::Null>(TEXT("match")))
	{
		Out = Match;
		return true;
	}

	const TSharedPtr<FJsonObject>* Object = nullptr;
	FString State;
	if (!Root->TryGetObjectField(TEXT("match"), Object) || !StringField(**Object, TEXT("id"), Match.MatchId) || !StringField(**Object, TEXT("state"), State))
	{
		OutProblem = TEXT("the match has no ID or state");
		return false;
	}
	if (!MatchesWhole(IdPattern, Match.MatchId))
	{
		OutProblem = TEXT("the match ID is not in the expected format");
		return false;
	}
	Match.bHasMatch = true;

	if (State.Equals(TEXT("allocating"), ESearchCase::CaseSensitive))
	{
		Out = Match;
		return true;
	}
	if (!State.Equals(TEXT("ready"), ESearchCase::CaseSensitive))
	{
		OutProblem = TEXT("the match's state is neither allocating nor ready");
		return false;
	}

	const TSharedPtr<FJsonObject>* Server = nullptr;
	double Port = 0.0;
	if (!(*Object)->TryGetObjectField(TEXT("server"), Server) || !StringField(**Server, TEXT("host"), Match.Host)
		|| !(*Server)->HasTypedField<EJson::Number>(TEXT("port")) || !(*Server)->TryGetNumberField(TEXT("port"), Port)
		|| !StringField(**Object, TEXT("ticket"), Match.Ticket))
	{
		OutProblem = TEXT("the ready match has no server address or ticket");
		return false;
	}
	if (!MatchesWhole(HostPattern, Match.Host) || Port != FMath::FloorToDouble(Port) || Port < 1.0 || Port > MaxPort)
	{
		OutProblem = TEXT("the match server's address is not a host and port");
		return false;
	}
	if (!MatchesWhole(JoinTicketPattern, Match.Ticket))
	{
		OutProblem = TEXT("the join ticket is not in the expected format");
		return false;
	}
	Match.Port = static_cast<int32>(Port);
	Match.bReady = true;
	Out = MoveTemp(Match);
	return true;
}

FString BuildResultBody(const FVeyraMatchResult& Result)
{
	FString Body;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("endReason"), EndReasonName(Result.EndReason));
	if (Result.Winner == EVeyraTeam::A || Result.Winner == EVeyraTeam::B)
	{
		Writer->WriteValue(TEXT("winner"), Result.Winner == EVeyraTeam::A ? TEXT("A") : TEXT("B"));
	}
	else
	{
		Writer->WriteNull(TEXT("winner"));
	}
	Writer->WriteValue(TEXT("durationSeconds"), Result.DurationSeconds);
	Writer->WriteArrayStart(TEXT("participants"));
	for (const FVeyraParticipantResult& Participant : Result.Participants)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("accountId"), Participant.AccountId);
		Writer->WriteValue(TEXT("joined"), Participant.bJoined);
		Writer->WriteValue(TEXT("connectedAtEnd"), Participant.bConnectedAtEnd);
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();
	Writer->WriteObjectEnd();
	Writer->Close();
	return Body;
}

FString ParseErrorCode(const FString& Body)
{
	const TSharedPtr<FJsonObject> Root = ParseObject(Body);
	FString Code;
	if (Root.IsValid() && StringField(*Root, TEXT("error"), Code) && MatchesWhole(ErrorCodePattern, Code))
	{
		return Code;
	}
	return FString();
}
}
