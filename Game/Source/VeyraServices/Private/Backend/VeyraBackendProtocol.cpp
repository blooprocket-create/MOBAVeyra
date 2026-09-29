// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Backend/VeyraBackendProtocol.h"

#include "Algo/Find.h"
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
	/** A content ID (Game/Tuning/README.md), such as a Vanguard's or a mode's. */
	const TCHAR* const ContentIdPattern = TEXT("^[a-z][a-z0-9]*(_[a-z0-9]+)*$");
	/** A word the backend uses for a state or a reason, such as "picking" or "timed_out". */
	const TCHAR* const WordPattern = TEXT("^[a-z_]{1,64}$");
	/** One of the battleground's two teams. */
	const TCHAR* const SidePattern = TEXT("^[AB]$");

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

	/** A string field in Pattern's format. */
	bool StringField(const FJsonObject& Object, FStringView Name, const TCHAR* Pattern, FString& Out)
	{
		return StringField(Object, Name, Out) && MatchesWhole(Pattern, Out);
	}

	/** A field that is null, which leaves Out empty, or a string in Pattern's format. */
	bool NullableStringField(const FJsonObject& Object, FStringView Name, const TCHAR* Pattern, FString& Out)
	{
		Out.Reset();
		return Object.HasTypedField<EJson::Null>(Name) || StringField(Object, Name, Pattern, Out);
	}

	bool BoolField(const FJsonObject& Object, FStringView Name, bool& Out)
	{
		return Object.HasTypedField<EJson::Boolean>(Name) && Object.TryGetBoolField(Name, Out);
	}

	/** A number field that is finite and not negative. */
	bool DurationField(const FJsonObject& Object, FStringView Name, double& Out)
	{
		return Object.HasTypedField<EJson::Number>(Name) && Object.TryGetNumberField(Name, Out) && FMath::IsFinite(Out) && Out >= 0.0;
	}

	/** An array of strings, each in Pattern's format. */
	bool StringArrayField(const FJsonObject& Object, FStringView Name, const TCHAR* Pattern, TArray<FString>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object.HasTypedField<EJson::Array>(Name) || !Object.TryGetArrayField(Name, Values))
		{
			return false;
		}
		Out.Reset();
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			FString Text;
			if (!Value.IsValid() || Value->Type != EJson::String || !Value->TryGetString(Text) || !MatchesWhole(Pattern, Text))
			{
				return false;
			}
			Out.Add(MoveTemp(Text));
		}
		return true;
	}

	/** An object field; null is not one. */
	const FJsonObject* ObjectField(const FJsonObject& Object, FStringView Name)
	{
		const TSharedPtr<FJsonObject>* Field = nullptr;
		return Object.HasTypedField<EJson::Object>(Name) && Object.TryGetObjectField(Name, Field) && Field->IsValid() ? Field->Get() : nullptr;
	}

	bool ParseSelectState(const FString& Text, ESelectState& Out)
	{
		static const TPair<const TCHAR*, ESelectState> States[] = {
			{ TEXT("picking"), ESelectState::Picking },
			{ TEXT("starting"), ESelectState::Starting },
			{ TEXT("started"), ESelectState::Started },
			{ TEXT("cancelled"), ESelectState::Cancelled },
		};
		for (const TPair<const TCHAR*, ESelectState>& State : States)
		{
			if (Text.Equals(State.Key, ESearchCase::CaseSensitive))
			{
				Out = State.Value;
				return true;
			}
		}
		return false;
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
		case EVeyraMatchEndReason::PrimeWellDestroyed:
			return TEXT("prime_well_destroyed");
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

bool IsContentId(FStringView Text)
{
	return MatchesWhole(ContentIdPattern, Text);
}

bool ParseProfile(const FString& Body, FProfile& Out, FString& OutProblem)
{
	const TSharedPtr<FJsonObject> Root = ParseObject(Body);
	const FJsonObject* Account = Root.IsValid() ? ObjectField(*Root, TEXT("account")) : nullptr;
	const FJsonObject* Tutorial = Root.IsValid() ? ObjectField(*Root, TEXT("tutorial")) : nullptr;
	FProfile Profile;
	if (!Account || !Tutorial || !StringField(*Account, TEXT("id"), IdPattern, Profile.AccountId) || !StringField(*Account, TEXT("displayName"), Profile.DisplayName)
		|| Profile.DisplayName.IsEmpty() || !BoolField(*Tutorial, TEXT("completed"), Profile.bTutorialCompleted)
		|| !NullableStringField(*Tutorial, TEXT("starterVanguardId"), ContentIdPattern, Profile.StarterVanguardId))
	{
		OutProblem = TEXT("the answer is not a profile with an account and a tutorial");
		return false;
	}
	Out = MoveTemp(Profile);
	return true;
}

bool ParseVanguardAccess(const FString& Body, FVanguardAccess& Out, FString& OutProblem)
{
	const TSharedPtr<FJsonObject> Root = ParseObject(Body);
	FVanguardAccess Access;
	if (!Root.IsValid() || !StringArrayField(*Root, TEXT("owned"), ContentIdPattern, Access.Owned)
		|| !StringArrayField(*Root, TEXT("rotation"), ContentIdPattern, Access.Rotation)
		|| !StringArrayField(*Root, TEXT("available"), ContentIdPattern, Access.Available)
		|| !StringArrayField(*Root, TEXT("starters"), ContentIdPattern, Access.Starters))
	{
		OutProblem = TEXT("the answer does not list owned, rotation, available and starter Vanguards by their IDs");
		return false;
	}
	Out = MoveTemp(Access);
	return true;
}

const FSelectSeat* FSelect::FindYou() const
{
	return Seats.FindByPredicate([](const FSelectSeat& Seat) { return Seat.bYou; });
}

bool ParseSelect(const FString& Body, TOptional<FSelect>& OutSelect, FString& OutProblem)
{
	const TSharedPtr<FJsonObject> Root = ParseObject(Body);
	if (!Root.IsValid() || !Root->HasField(TEXT("select")))
	{
		OutProblem = TEXT("the answer has no \"select\"");
		return false;
	}
	if (Root->HasTypedField<EJson::Null>(TEXT("select")))
	{
		OutSelect.Reset();
		return true;
	}

	const FJsonObject* Object = ObjectField(*Root, TEXT("select"));
	FSelect Select;
	FString State;
	const TArray<TSharedPtr<FJsonValue>>* Seats = nullptr;
	if (!Object || !StringField(*Object, TEXT("id"), IdPattern, Select.Id) || !StringField(*Object, TEXT("kind"), WordPattern, Select.Kind)
		|| !StringField(*Object, TEXT("mode"), ContentIdPattern, Select.Mode) || !StringField(*Object, TEXT("state"), State)
		|| !ParseSelectState(State, Select.State) || !DurationField(*Object, TEXT("remainingSeconds"), Select.RemainingSeconds)
		|| !NullableStringField(*Object, TEXT("matchId"), IdPattern, Select.MatchId)
		|| !NullableStringField(*Object, TEXT("cancelReason"), WordPattern, Select.CancelReason)
		|| !Object->HasTypedField<EJson::Array>(TEXT("seats")) || !Object->TryGetArrayField(TEXT("seats"), Seats))
	{
		OutProblem = TEXT("the select's ID, kind, mode, state, timer, match, cancel reason or seats are missing or not in the expected format");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : *Seats)
	{
		const TSharedPtr<FJsonObject>* SeatObject = nullptr;
		FSelectSeat Seat;
		if (!Value.IsValid() || !Value->TryGetObject(SeatObject) || !SeatObject->IsValid() || !StringField(**SeatObject, TEXT("displayName"), Seat.DisplayName)
			|| Seat.DisplayName.IsEmpty() || !StringField(**SeatObject, TEXT("side"), SidePattern, Seat.Side) || !BoolField(**SeatObject, TEXT("you"), Seat.bYou)
			|| !NullableStringField(**SeatObject, TEXT("hover"), ContentIdPattern, Seat.Hover)
			|| !NullableStringField(**SeatObject, TEXT("locked"), ContentIdPattern, Seat.Locked))
		{
			OutProblem = TEXT("a seat of the select is not in the expected format");
			return false;
		}
		Select.Seats.Add(MoveTemp(Seat));
	}
	if (Select.Seats.FilterByPredicate([](const FSelectSeat& Seat) { return Seat.bYou; }).Num() != 1)
	{
		OutProblem = TEXT("the select does not hold exactly one seat for this player");
		return false;
	}
	if ((Select.State == ESelectState::Started) == Select.MatchId.IsEmpty() || (Select.State == ESelectState::Cancelled) == Select.CancelReason.IsEmpty())
	{
		OutProblem = TEXT("the select's match or cancel reason does not fit its state");
		return false;
	}
	OutSelect = MoveTemp(Select);
	return true;
}

bool FMatchOutcome::IsActive() const
{
	return State.Equals(TEXT("allocating"), ESearchCase::CaseSensitive) || State.Equals(TEXT("ready"), ESearchCase::CaseSensitive);
}

bool ParseMatchOutcome(const FString& Body, FMatchOutcome& Out, FString& OutProblem)
{
	const TSharedPtr<FJsonObject> Root = ParseObject(Body);
	const FJsonObject* Object = Root.IsValid() ? ObjectField(*Root, TEXT("match")) : nullptr;
	FMatchOutcome Outcome;
	if (!Object || !StringField(*Object, TEXT("id"), IdPattern, Outcome.MatchId) || !StringField(*Object, TEXT("mode"), ContentIdPattern, Outcome.Mode)
		|| !StringField(*Object, TEXT("rules"), WordPattern, Outcome.Rules) || !StringField(*Object, TEXT("state"), WordPattern, Outcome.State)
		|| !StringField(*Object, TEXT("side"), SidePattern, Outcome.Side) || !NullableStringField(*Object, TEXT("vanguardId"), ContentIdPattern, Outcome.VanguardId)
		|| !NullableStringField(*Object, TEXT("failureReason"), WordPattern, Outcome.FailureReason) || !Object->HasField(TEXT("result")))
	{
		OutProblem = TEXT("the match's ID, mode, rules, state, side, Vanguard, failure or result are missing or not in the expected format");
		return false;
	}
	if (const FJsonObject* Result = ObjectField(*Object, TEXT("result")))
	{
		if (!StringField(*Result, TEXT("endReason"), WordPattern, Outcome.EndReason) || !NullableStringField(*Result, TEXT("winner"), SidePattern, Outcome.Winner)
			|| !DurationField(*Result, TEXT("durationSeconds"), Outcome.DurationSeconds) || !BoolField(*Result, TEXT("joined"), Outcome.bJoined)
			|| !BoolField(*Result, TEXT("connectedAtEnd"), Outcome.bConnectedAtEnd))
		{
			OutProblem = TEXT("the match's result is not in the expected format");
			return false;
		}
		Outcome.bHasResult = true;
	}
	else if (!Object->HasTypedField<EJson::Null>(TEXT("result")))
	{
		OutProblem = TEXT("the match's result is neither null nor a result");
		return false;
	}
	Out = MoveTemp(Outcome);
	return true;
}

bool ParseModes(const FString& Body, TArray<FModeInfo>& Out, FString& OutProblem)
{
	const TSharedPtr<FJsonObject> Root = ParseObject(Body);
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Root.IsValid() || !Root->HasTypedField<EJson::Array>(TEXT("modes")) || !Root->TryGetArrayField(TEXT("modes"), Values))
	{
		OutProblem = TEXT("the answer has no list of modes");
		return false;
	}
	TArray<FModeInfo> Modes;
	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		FModeInfo Mode;
		FString Matchmaking;
		double Team = 0.0;
		if (!Value.IsValid() || !Value->TryGetObject(Object) || !Object->IsValid() || !StringField(**Object, TEXT("id"), ContentIdPattern, Mode.Id)
			|| !BoolField(**Object, TEXT("enabled"), Mode.bEnabled) || !DurationField(**Object, TEXT("humanPlayersPerTeam"), Team) || Team < 1.0
			|| !StringField(**Object, TEXT("matchmaking"), Matchmaking))
		{
			OutProblem = TEXT("a mode is not in the expected format");
			return false;
		}
		Mode.HumanPlayersPerTeam = FMath::FloorToInt32(Team);
		Mode.bMatchmade = Matchmaking.Equals(TEXT("casualSelect"), ESearchCase::CaseSensitive);
		Modes.Add(MoveTemp(Mode));
	}
	Out = MoveTemp(Modes);
	return true;
}

const FPartyMember* FParty::Find(const FString& AccountId) const
{
	return Members.FindByPredicate([&AccountId](const FPartyMember& Member) { return Member.AccountId == AccountId; });
}

bool FParty::AllReady() const
{
	return !Members.ContainsByPredicate([](const FPartyMember& Member) { return !Member.bReady; });
}

bool ParseParty(const FString& Body, TOptional<FParty>& OutParty, FString& OutProblem)
{
	const TSharedPtr<FJsonObject> Root = ParseObject(Body);
	if (!Root.IsValid() || !Root->HasField(TEXT("party")))
	{
		OutProblem = TEXT("the answer has no \"party\"");
		return false;
	}
	if (Root->HasTypedField<EJson::Null>(TEXT("party")))
	{
		OutParty.Reset();
		return true;
	}
	const FJsonObject* Object = ObjectField(*Root, TEXT("party"));
	FParty Party;
	FString Status;
	const TArray<TSharedPtr<FJsonValue>>* Members = nullptr;
	if (!Object || !StringField(*Object, TEXT("id"), IdPattern, Party.Id) || !StringField(*Object, TEXT("mode"), Party.Mode)
		|| (!Party.Mode.IsEmpty() && !MatchesWhole(ContentIdPattern, Party.Mode)) || !StringField(*Object, TEXT("status"), Status)
		|| !DurationField(*Object, TEXT("queuedSeconds"), Party.QueuedSeconds) || !Object->HasTypedField<EJson::Array>(TEXT("members"))
		|| !Object->TryGetArrayField(TEXT("members"), Members))
	{
		OutProblem = TEXT("the party's ID, mode, status, queue time or members are missing or not in the expected format");
		return false;
	}
	static const TPair<const TCHAR*, EPartyStatus> Statuses[] = {
		{ TEXT("idle"), EPartyStatus::Idle },
		{ TEXT("queued"), EPartyStatus::Queued },
		{ TEXT("found"), EPartyStatus::Found },
		{ TEXT("selecting"), EPartyStatus::Selecting },
	};
	const TPair<const TCHAR*, EPartyStatus>* Known = Algo::FindByPredicate(Statuses, [&Status](const TPair<const TCHAR*, EPartyStatus>& Candidate) {
		return Status.Equals(Candidate.Key, ESearchCase::CaseSensitive);
	});
	if (!Known)
	{
		OutProblem = TEXT("the party's status is not one the game knows");
		return false;
	}
	Party.Status = Known->Value;
	for (const TSharedPtr<FJsonValue>& Value : *Members)
	{
		const TSharedPtr<FJsonObject>* MemberObject = nullptr;
		FPartyMember Member;
		if (!Value.IsValid() || !Value->TryGetObject(MemberObject) || !MemberObject->IsValid()
			|| !StringField(**MemberObject, TEXT("accountId"), IdPattern, Member.AccountId) || !StringField(**MemberObject, TEXT("displayName"), Member.DisplayName)
			|| Member.DisplayName.IsEmpty() || !BoolField(**MemberObject, TEXT("ready"), Member.bReady) || !BoolField(**MemberObject, TEXT("leader"), Member.bLeader))
		{
			OutProblem = TEXT("a member of the party is not in the expected format");
			return false;
		}
		Party.Members.Add(MoveTemp(Member));
	}
	if (Party.Members.FilterByPredicate([](const FPartyMember& Member) { return Member.bLeader; }).Num() != 1)
	{
		OutProblem = TEXT("the party does not have exactly one leader");
		return false;
	}
	OutParty = MoveTemp(Party);
	return true;
}

bool ParseMatchFound(const FString& Body, TOptional<FMatchFound>& OutFound, FString& OutProblem)
{
	const TSharedPtr<FJsonObject> Root = ParseObject(Body);
	if (!Root.IsValid() || !Root->HasField(TEXT("matchFound")))
	{
		OutProblem = TEXT("the answer has no \"matchFound\"");
		return false;
	}
	if (Root->HasTypedField<EJson::Null>(TEXT("matchFound")))
	{
		OutFound.Reset();
		return true;
	}
	const FJsonObject* Object = ObjectField(*Root, TEXT("matchFound"));
	FMatchFound Found;
	double Accepted = 0.0;
	double Total = 0.0;
	if (!Object || !StringField(*Object, TEXT("id"), IdPattern, Found.Id) || !StringField(*Object, TEXT("mode"), ContentIdPattern, Found.Mode)
		|| !StringField(*Object, TEXT("state"), WordPattern, Found.State) || !DurationField(*Object, TEXT("remainingSeconds"), Found.RemainingSeconds)
		|| !DurationField(*Object, TEXT("accepted"), Accepted) || !DurationField(*Object, TEXT("total"), Total) || Accepted > Total || Total < 1.0
		|| !StringField(*Object, TEXT("you"), WordPattern, Found.You) || !NullableStringField(*Object, TEXT("selectId"), IdPattern, Found.SelectId)
		|| !NullableStringField(*Object, TEXT("abandonReason"), WordPattern, Found.AbandonReason))
	{
		OutProblem = TEXT("the match found's ID, mode, state, timer, counts, answer, select or reason are missing or not in the expected format");
		return false;
	}
	Found.Accepted = FMath::FloorToInt32(Accepted);
	Found.Total = FMath::FloorToInt32(Total);
	OutFound = MoveTemp(Found);
	return true;
}

FString BuildModeBody(const FString& ModeId)
{
	FString Body;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("mode"), ModeId);
	Writer->WriteObjectEnd();
	Writer->Close();
	return Body;
}

FString BuildReadyBody(bool bReady)
{
	FString Body;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("ready"), bReady);
	Writer->WriteObjectEnd();
	Writer->Close();
	return Body;
}

FString BuildVanguardBody(const FString& VanguardId)
{
	FString Body;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("vanguardId"), VanguardId);
	Writer->WriteObjectEnd();
	Writer->Close();
	return Body;
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
