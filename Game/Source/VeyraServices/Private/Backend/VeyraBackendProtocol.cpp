// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Backend/VeyraBackendProtocol.h"

#include "Algo/Find.h"
#include "Dom/JsonObject.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Internationalization/Regex.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Slots/VeyraAbilitySlot.h"

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
	/** A player's outcome in a completed match (Backend/internal/match/history.go). */
	const TCHAR* const OutcomePattern = TEXT("^(win|loss|no_contest)$");
	/** Match History's cursor: unpadded base64url, opaque. */
	const TCHAR* const CursorPattern = TEXT("^[A-Za-z0-9_-]{1,256}$");

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

	/** A boolean field that may be absent, which leaves Out as it was. */
	bool OptionalBoolField(const FJsonObject& Object, FStringView Name, bool& Out)
	{
		return !Object.HasField(Name) || BoolField(Object, Name, Out);
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

	/**
	 * The scoreboard's statistics as the backend names them (Backend/internal/match/scoreboard.go;
	 * ADR-017 §5): each a count or an amount, at the top of "statistics" or in one of its groups.
	 * The result body and the verified result both use this one table, so they cannot disagree.
	 */
	struct FStatisticCount
	{
		const TCHAR* Name;
		int32& (*Of)(FVeyraPlayerStatistics&);
	};

	struct FStatisticAmount
	{
		/** The group object it sits in, such as "damageDealt"; null at the top. */
		const TCHAR* Group;
		const TCHAR* Name;
		double& (*Of)(FVeyraPlayerStatistics&);
	};

	using FStats = FVeyraPlayerStatistics;

	const FStatisticCount StatisticCounts[] = {
		{ TEXT("kills"), [](FStats& S) -> int32& { return S.Kills; } },
		{ TEXT("deaths"), [](FStats& S) -> int32& { return S.Deaths; } },
		{ TEXT("assists"), [](FStats& S) -> int32& { return S.Assists; } },
		{ TEXT("level"), [](FStats& S) -> int32& { return S.Level; } },
		{ TEXT("minionKills"), [](FStats& S) -> int32& { return S.MinionKills; } },
		{ TEXT("jungleKills"), [](FStats& S) -> int32& { return S.JungleKills; } },
		{ TEXT("wellsSecured"), [](FStats& S) -> int32& { return S.WellsSecured; } },
		{ TEXT("wellFinalHits"), [](FStats& S) -> int32& { return S.WellFinalHits; } },
		{ TEXT("wardsPlaced"), [](FStats& S) -> int32& { return S.WardsPlaced; } },
		{ TEXT("wardsDestroyed"), [](FStats& S) -> int32& { return S.WardsDestroyed; } },
		{ TEXT("buybacks"), [](FStats& S) -> int32& { return S.Buybacks; } },
	};

	const FStatisticAmount StatisticAmounts[] = {
		{ nullptr, TEXT("vanguardDamage"), [](FStats& S) -> double& { return S.VanguardDamage; } },
		{ nullptr, TEXT("damageShielded"), [](FStats& S) -> double& { return S.DamageShielded; } },
		{ nullptr, TEXT("selfHealing"), [](FStats& S) -> double& { return S.SelfHealing; } },
		{ nullptr, TEXT("teammateHealing"), [](FStats& S) -> double& { return S.TeammateHealing; } },
		{ nullptr, TEXT("goldEarned"), [](FStats& S) -> double& { return S.GoldEarned; } },
		{ nullptr, TEXT("towerDamage"), [](FStats& S) -> double& { return S.TowerDamage; } },
		{ nullptr, TEXT("wellDamage"), [](FStats& S) -> double& { return S.WellDamage; } },
		{ TEXT("damageDealt"), TEXT("physical"), [](FStats& S) -> double& { return S.DamageDealt.Physical; } },
		{ TEXT("damageDealt"), TEXT("magic"), [](FStats& S) -> double& { return S.DamageDealt.Magic; } },
		{ TEXT("damageDealt"), TEXT("true"), [](FStats& S) -> double& { return S.DamageDealt.TrueDamage; } },
		{ TEXT("damageTaken"), TEXT("physical"), [](FStats& S) -> double& { return S.DamageTaken.Physical; } },
		{ TEXT("damageTaken"), TEXT("magic"), [](FStats& S) -> double& { return S.DamageTaken.Magic; } },
		{ TEXT("damageTaken"), TEXT("true"), [](FStats& S) -> double& { return S.DamageTaken.TrueDamage; } },
		{ TEXT("crowdControl"), TEXT("stun"), [](FStats& S) -> double& { return S.CrowdControl.Stun; } },
		{ TEXT("crowdControl"), TEXT("slow"), [](FStats& S) -> double& { return S.CrowdControl.Slow; } },
		{ TEXT("crowdControl"), TEXT("total"), [](FStats& S) -> double& { return S.CrowdControl.Total; } },
		{ TEXT("goldBySource"), TEXT("starting"), [](FStats& S) -> double& { return S.GoldBySource.Starting; } },
		{ TEXT("goldBySource"), TEXT("kills"), [](FStats& S) -> double& { return S.GoldBySource.Kills; } },
		{ TEXT("goldBySource"), TEXT("assists"), [](FStats& S) -> double& { return S.GoldBySource.Assists; } },
		{ TEXT("goldBySource"), TEXT("minions"), [](FStats& S) -> double& { return S.GoldBySource.Minions; } },
		{ TEXT("goldBySource"), TEXT("jungle"), [](FStats& S) -> double& { return S.GoldBySource.Jungle; } },
		{ TEXT("goldBySource"), TEXT("objectives"), [](FStats& S) -> double& { return S.GoldBySource.Objectives; } },
		{ TEXT("goldBySource"), TEXT("wards"), [](FStats& S) -> double& { return S.GoldBySource.Wards; } },
		{ TEXT("goldBySource"), TEXT("passive"), [](FStats& S) -> double& { return S.GoldBySource.Passive; } },
	};

	/** The groups "statistics" holds, in the order the body writes them. */
	const TCHAR* const StatisticGroups[] = { TEXT("damageDealt"), TEXT("damageTaken"), TEXT("crowdControl"), TEXT("goldBySource") };

	/** The spell slots a line carries, which the backend requires exactly (ADR-015 §1). */
	constexpr int32 SpellSlotCount = UE_ARRAY_COUNT(VeyraAbilitySlots::Spells);

	using FBodyWriter = TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>;

	void WriteStatistics(FBodyWriter& Writer, FVeyraPlayerStatistics Values)
	{
		Writer.WriteObjectStart(TEXT("statistics"));
		for (const FStatisticCount& Count : StatisticCounts)
		{
			Writer.WriteValue(Count.Name, Count.Of(Values));
		}
		for (const FStatisticAmount& Amount : StatisticAmounts)
		{
			if (!Amount.Group)
			{
				Writer.WriteValue(Amount.Name, Amount.Of(Values));
			}
		}
		for (const TCHAR* Group : StatisticGroups)
		{
			Writer.WriteObjectStart(Group);
			for (const FStatisticAmount& Amount : StatisticAmounts)
			{
				if (Amount.Group && FCString::Strcmp(Amount.Group, Group) == 0)
				{
					Writer.WriteValue(Amount.Name, Amount.Of(Values));
				}
			}
			Writer.WriteObjectEnd();
		}
		Writer.WriteObjectEnd();
	}

	/** Slots as the backend takes them: a content ID, or "" for an empty slot, at least MinSlots of them. */
	void WriteSlots(FBodyWriter& Writer, const TCHAR* Name, TConstArrayView<FVeyraContentId> Slots, int32 MinSlots)
	{
		Writer.WriteArrayStart(Name);
		for (int32 Index = 0; Index < FMath::Max(Slots.Num(), MinSlots); ++Index)
		{
			Writer.WriteValue(Slots.IsValidIndex(Index) && Slots[Index].IsValid() ? Slots[Index].ToString() : FString());
		}
		Writer.WriteArrayEnd();
	}

	/** A number field that is a whole count, not negative. */
	bool CountField(const FJsonObject& Object, FStringView Name, int32& Out)
	{
		double Value = 0.0;
		if (!DurationField(Object, Name, Value) || Value != FMath::FloorToDouble(Value) || Value > MAX_int32)
		{
			return false;
		}
		Out = static_cast<int32>(Value);
		return true;
	}

	bool ParseStatistics(const FJsonObject& Object, FVeyraPlayerStatistics& Out)
	{
		for (const FStatisticCount& Count : StatisticCounts)
		{
			if (!CountField(Object, Count.Name, Count.Of(Out)))
			{
				return false;
			}
		}
		for (const FStatisticAmount& Amount : StatisticAmounts)
		{
			const FJsonObject* Group = Amount.Group ? ObjectField(Object, Amount.Group) : &Object;
			if (!Group || !DurationField(*Group, Amount.Name, Amount.Of(Out)))
			{
				return false;
			}
		}
		return true;
	}

	/** Slots from an array of strings, each a content ID or "" for an empty slot. */
	bool SlotsField(const FJsonObject& Object, FStringView Name, TArray<FVeyraContentId>& Out)
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
			if (!Value.IsValid() || Value->Type != EJson::String || !Value->TryGetString(Text))
			{
				return false;
			}
			const TOptional<FVeyraContentId> Slot = Text.IsEmpty() ? TOptional<FVeyraContentId>(FVeyraContentId()) : FVeyraContentId::FromText(Text);
			if (!Slot.IsSet())
			{
				return false;
			}
			Out.Add(Slot.GetValue());
		}
		return true;
	}

	/** A verified result's scoreboard line (Backend/internal/httpapi/match.go, scoreboardPlayerJSON). */
	bool ParsePlayerOutcome(const FJsonValue& Value, FPlayerOutcome& Out)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (Value.Type != EJson::Object || !Value.TryGetObject(Object) || !Object->IsValid())
		{
			return false;
		}
		const FJsonObject& Line = **Object;
		const FJsonObject* Statistics = ObjectField(Line, TEXT("statistics"));
		return StringField(Line, TEXT("side"), SidePattern, Out.Side) && StringField(Line, TEXT("name"), Out.Name) && !Out.Name.IsEmpty()
			&& StringField(Line, TEXT("vanguardId"), ContentIdPattern, Out.VanguardId) && BoolField(Line, TEXT("you"), Out.bYou) && Statistics
			&& ParseStatistics(*Statistics, Out.Statistics) && SlotsField(Line, TEXT("items"), Out.Statistics.Items)
			&& SlotsField(Line, TEXT("fluxSpells"), Out.Statistics.FluxSpells) && Out.Statistics.FluxSpells.Num() == SpellSlotCount;
	}

	/** A Flux Well secured, as the backend returns it: its site, its side and when. */
	bool ParseWellOutcome(const FJsonValue& Value, FWellOutcome& Out)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		double AtSeconds = 0.0;
		const bool bParsed = Value.Type == EJson::Object && Value.TryGetObject(Object) && Object->IsValid() && CountField(**Object, TEXT("site"), Out.Site)
			&& StringField(**Object, TEXT("side"), SidePattern, Out.Side) && DurationField(**Object, TEXT("atSeconds"), AtSeconds);
		Out.AtSeconds = AtSeconds;
		return bParsed;
	}

	bool ParseSelectPhase(const FString& Text, ESelectPhase& Out)
	{
		static const TPair<const TCHAR*, ESelectPhase> Phases[] = {
			{ TEXT("banning"), ESelectPhase::Banning },
			{ TEXT("picking"), ESelectPhase::Picking },
			{ TEXT("final"), ESelectPhase::Final },
		};
		for (const TPair<const TCHAR*, ESelectPhase>& Phase : Phases)
		{
			if (Text.Equals(Phase.Key, ESearchCase::CaseSensitive))
			{
				Out = Phase.Value;
				return true;
			}
		}
		return false;
	}

	/** A draft's phase, turn and bans (ADR-042 §1). Older selects left them out: one picking phase, no turns, no bans. */
	bool ParseDraft(const FJsonObject& Object, FSelect& Select, FString& OutProblem)
	{
		FString Phase;
		if (Object.HasField(TEXT("phase")) && (!StringField(Object, TEXT("phase"), Phase) || !ParseSelectPhase(Phase, Select.Phase)))
		{
			OutProblem = TEXT("the select's phase is not one the game knows");
			return false;
		}
		if (Object.HasField(TEXT("turn")) && !Object.HasTypedField<EJson::Null>(TEXT("turn")))
		{
			const FJsonObject* TurnObject = ObjectField(Object, TEXT("turn"));
			FSelectTurn Turn;
			if (!TurnObject || !BoolField(*TurnObject, TEXT("ban"), Turn.bBan) || !StringField(*TurnObject, TEXT("side"), SidePattern, Turn.Side)
				|| !CountField(*TurnObject, TEXT("count"), Turn.Count) || !CountField(*TurnObject, TEXT("done"), Turn.Done) || Turn.Count < 1
				|| Turn.Done >= Turn.Count)
			{
				OutProblem = TEXT("the select's turn is not in the expected format");
				return false;
			}
			Select.Turn = MoveTemp(Turn);
		}
		if ((Select.Phase == ESelectPhase::Banning) != (Select.Turn.IsSet() && Select.Turn->bBan) || (Select.Phase == ESelectPhase::Final && Select.Turn.IsSet()))
		{
			OutProblem = TEXT("the select's turn does not fit its phase");
			return false;
		}
		const TArray<TSharedPtr<FJsonValue>>* Bans = nullptr;
		if (Object.HasTypedField<EJson::Array>(TEXT("bans")) && Object.TryGetArrayField(TEXT("bans"), Bans))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Bans)
			{
				const TSharedPtr<FJsonObject>* BanObject = nullptr;
				FSelectBan Ban;
				if (!Value.IsValid() || !Value->TryGetObject(BanObject) || !BanObject->IsValid() || !StringField(**BanObject, TEXT("side"), SidePattern, Ban.Side)
					|| !StringField(**BanObject, TEXT("vanguardId"), ContentIdPattern, Ban.VanguardId))
				{
					OutProblem = TEXT("a ban of the select is not in the expected format");
					return false;
				}
				Select.Bans.Add(MoveTemp(Ban));
			}
		}
		else if (Object.HasField(TEXT("bans")))
		{
			OutProblem = TEXT("the select's bans are not a list");
			return false;
		}
		return true;
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
		case EVeyraMatchEndReason::Surrender:
			return TEXT("surrender");
		case EVeyraMatchEndReason::Remake:
			return TEXT("remake");
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
		|| !StringArrayField(*Root, TEXT("starters"), ContentIdPattern, Access.Starters)
		// Older backends did not list the released Vanguards.
		|| (Root->HasField(TEXT("released")) && !StringArrayField(*Root, TEXT("released"), ContentIdPattern, Access.Released)))
	{
		OutProblem = TEXT("the answer does not list owned, rotation, available, starter and released Vanguards by their IDs");
		return false;
	}
	Out = MoveTemp(Access);
	return true;
}

const FSelectSeat* FSelect::FindYou() const
{
	return Seats.FindByPredicate([](const FSelectSeat& Seat) { return Seat.bYou; });
}

bool FSelect::YouBan() const
{
	const FSelectSeat* You = FindYou();
	return State == ESelectState::Picking && Turn.IsSet() && Turn->bBan && You && You->bActing;
}

bool FSelect::YouMayLock() const
{
	const FSelectSeat* You = FindYou();
	if (State != ESelectState::Picking || !You || !You->Locked.IsEmpty() || Phase == ESelectPhase::Final)
	{
		return false;
	}
	// Outside a draft every seat picks at once.
	return !Turn.IsSet() || (!Turn->bBan && You->bActing);
}

bool FSelect::IsBanned(const FString& VanguardId) const
{
	return Bans.ContainsByPredicate([&VanguardId](const FSelectBan& Ban) { return Ban.VanguardId == VanguardId; });
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
		|| !DurationField(*Object, TEXT("pickSeconds"), Select.PickSeconds)
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
			|| !NullableStringField(**SeatObject, TEXT("locked"), ContentIdPattern, Seat.Locked)
			// A draft's and a trade's details (ADR-042), which older selects left out.
			|| ((*SeatObject)->HasField(TEXT("banHover")) && !NullableStringField(**SeatObject, TEXT("banHover"), ContentIdPattern, Seat.BanHover))
			|| !OptionalBoolField(**SeatObject, TEXT("acting"), Seat.bActing) || !OptionalBoolField(**SeatObject, TEXT("offersYou"), Seat.bOffersYou)
			|| !OptionalBoolField(**SeatObject, TEXT("offeredByYou"), Seat.bOfferedByYou))
		{
			OutProblem = TEXT("a seat of the select is not in the expected format");
			return false;
		}
		// Only the player's own seat carries its Flux Spells: content IDs, or "" for an empty slot.
		const TArray<TSharedPtr<FJsonValue>>* Spells = nullptr;
		if (!(*SeatObject)->HasTypedField<EJson::Null>(TEXT("fluxSpells")) && (*SeatObject)->TryGetArrayField(TEXT("fluxSpells"), Spells))
		{
			for (const TSharedPtr<FJsonValue>& Spell : *Spells)
			{
				FString Id;
				if (!Spell.IsValid() || !Spell->TryGetString(Id) || (!Id.IsEmpty() && !MatchesWhole(ContentIdPattern, Id)))
				{
					OutProblem = TEXT("a seat's Flux Spells are not in the expected format");
					return false;
				}
				Seat.FluxSpells.Add(MoveTemp(Id));
			}
		}
		Select.Seats.Add(MoveTemp(Seat));
	}
	// A custom select's bots (ADR-021 §3). Every other kind has none, and the older selects left the field out.
	const TArray<TSharedPtr<FJsonValue>>* Bots = nullptr;
	if (Object->HasTypedField<EJson::Array>(TEXT("bots")) && Object->TryGetArrayField(TEXT("bots"), Bots))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Bots)
		{
			const TSharedPtr<FJsonObject>* BotObject = nullptr;
			FSelectBot Bot;
			if (!Value.IsValid() || !Value->TryGetObject(BotObject) || !BotObject->IsValid() || !StringField(**BotObject, TEXT("side"), SidePattern, Bot.Side)
				|| !StringField(**BotObject, TEXT("vanguardId"), ContentIdPattern, Bot.VanguardId)
				|| !StringField(**BotObject, TEXT("difficulty"), WordPattern, Bot.Difficulty))
			{
				OutProblem = TEXT("a bot of the select is not in the expected format");
				return false;
			}
			Select.Bots.Add(MoveTemp(Bot));
		}
	}
	else if (Object->HasField(TEXT("bots")))
	{
		OutProblem = TEXT("the select's bots are not a list");
		return false;
	}
	if (!ParseDraft(*Object, Select, OutProblem))
	{
		return false;
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
		// A personal loss for absence (ADR-019 §5); absent from an older backend.
		Result->TryGetBoolField(TEXT("personalLoss"), Outcome.bPersonalLoss);
		// The scoreboard: null, or absent from an older backend, when the server sent none (ADR-017 §5).
		const TArray<TSharedPtr<FJsonValue>>* Players = nullptr;
		if (Result->HasTypedField<EJson::Array>(TEXT("players")) && Result->TryGetArrayField(TEXT("players"), Players))
		{
			for (const TSharedPtr<FJsonValue>& Line : *Players)
			{
				if (!Line.IsValid() || !ParsePlayerOutcome(*Line, Outcome.Players.AddDefaulted_GetRef()))
				{
					OutProblem = TEXT("the match's scoreboard is not in the expected format");
					return false;
				}
			}
			Outcome.bHasScoreboard = true;
		}
		else if (Result->HasField(TEXT("players")) && !Result->HasTypedField<EJson::Null>(TEXT("players")))
		{
			OutProblem = TEXT("the match's scoreboard is neither null nor a list");
			return false;
		}
		// The Flux Wells secured: null, or absent, when the server sent none.
		const TArray<TSharedPtr<FJsonValue>>* Wells = nullptr;
		if (Result->HasTypedField<EJson::Array>(TEXT("wells")) && Result->TryGetArrayField(TEXT("wells"), Wells))
		{
			for (const TSharedPtr<FJsonValue>& Capture : *Wells)
			{
				if (!Capture.IsValid() || !ParseWellOutcome(*Capture, Outcome.Wells.AddDefaulted_GetRef()))
				{
					OutProblem = TEXT("the match's Flux Wells are not in the expected format");
					return false;
				}
			}
		}
		else if (Result->HasField(TEXT("wells")) && !Result->HasTypedField<EJson::Null>(TEXT("wells")))
		{
			OutProblem = TEXT("the match's Flux Wells are neither null nor a list");
			return false;
		}
	}
	else if (!Object->HasTypedField<EJson::Null>(TEXT("result")))
	{
		OutProblem = TEXT("the match's result is neither null nor a result");
		return false;
	}
	Out = MoveTemp(Outcome);
	return true;
}

FString HistoryPath(const FHistoryFilter& Filter, const FString& Cursor)
{
	TArray<FString> Query;
	const TPair<const TCHAR*, const FString*> Fields[] = {
		{ TEXT("vanguard"), &Filter.VanguardId },
		{ TEXT("mode"), &Filter.Mode },
		{ TEXT("outcome"), &Filter.Outcome },
		{ TEXT("cursor"), &Cursor },
	};
	for (const TPair<const TCHAR*, const FString*>& Field : Fields)
	{
		if (!Field.Value->IsEmpty())
		{
			Query.Add(FString::Printf(TEXT("%s=%s"), Field.Key, *FGenericPlatformHttp::UrlEncode(*Field.Value)));
		}
	}
	return Query.IsEmpty() ? FString(TEXT("/v1/me/matches")) : TEXT("/v1/me/matches?") + FString::Join(Query, TEXT("&"));
}

bool ParseHistoryPage(const FString& Body, FHistoryPage& Out, FString& OutProblem)
{
	const TSharedPtr<FJsonObject> Root = ParseObject(Body);
	const TArray<TSharedPtr<FJsonValue>>* Matches = nullptr;
	FHistoryPage Page;
	if (!Root.IsValid() || !Root->HasTypedField<EJson::Array>(TEXT("matches")) || !Root->TryGetArrayField(TEXT("matches"), Matches)
		|| !NullableStringField(*Root, TEXT("next"), CursorPattern, Page.Next))
	{
		OutProblem = TEXT("the match history's list or cursor is missing or not in the expected format");
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Modes = nullptr;
	if (!Root->HasTypedField<EJson::Array>(TEXT("modes")) || !Root->TryGetArrayField(TEXT("modes"), Modes))
	{
		OutProblem = TEXT("the match history's modes are missing");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : *Modes)
	{
		FString Mode;
		if (!Value.IsValid() || !Value->TryGetString(Mode) || !MatchesWhole(ContentIdPattern, Mode))
		{
			OutProblem = TEXT("a mode in the match history is not a content ID");
			return false;
		}
		Page.Modes.Add(MoveTemp(Mode));
	}
	for (const TSharedPtr<FJsonValue>& Value : *Matches)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		FHistoryEntry& Entry = Page.Entries.AddDefaulted_GetRef();
		FString EndedAt;
		if (!Value.IsValid() || Value->Type != EJson::Object || !Value->TryGetObject(Object) || !Object->IsValid()
			|| !StringField(**Object, TEXT("id"), IdPattern, Entry.MatchId) || !StringField(**Object, TEXT("mode"), ContentIdPattern, Entry.Mode)
			|| !StringField(**Object, TEXT("rules"), WordPattern, Entry.Rules) || !StringField(**Object, TEXT("endedAt"), EndedAt)
			|| !FDateTime::ParseIso8601(*EndedAt, Entry.EndedAt) || !DurationField(**Object, TEXT("durationSeconds"), Entry.DurationSeconds)
			|| !StringField(**Object, TEXT("side"), SidePattern, Entry.Side) || !NullableStringField(**Object, TEXT("vanguardId"), ContentIdPattern, Entry.VanguardId)
			|| !StringField(**Object, TEXT("outcome"), OutcomePattern, Entry.Outcome))
		{
			OutProblem = TEXT("a match in the history is not in the expected format");
			return false;
		}
		// A personal loss for absence (UX-51); absent from an older backend.
		(*Object)->TryGetBoolField(TEXT("personalLoss"), Entry.bPersonalLoss);
	}
	Out = MoveTemp(Page);
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
		FString Category;
		double Team = 0.0;
		if (!Value.IsValid() || !Value->TryGetObject(Object) || !Object->IsValid() || !StringField(**Object, TEXT("id"), ContentIdPattern, Mode.Id)
			|| !BoolField(**Object, TEXT("enabled"), Mode.bEnabled) || !DurationField(**Object, TEXT("humanPlayersPerTeam"), Team) || Team < 1.0
			|| !StringField(**Object, TEXT("matchmaking"), Matchmaking) || !StringField(**Object, TEXT("category"), Category))
		{
			OutProblem = TEXT("a mode is not in the expected format");
			return false;
		}
		if (Category.Equals(TEXT("ranked"), ESearchCase::CaseSensitive))
		{
			Mode.Category = EModeCategory::Ranked;
		}
		else if (Category.Equals(TEXT("casual"), ESearchCase::CaseSensitive))
		{
			Mode.Category = EModeCategory::Casual;
		}
		else if (Category.Equals(TEXT("ai"), ESearchCase::CaseSensitive))
		{
			Mode.Category = EModeCategory::AI;
		}
		else
		{
			OutProblem = FString::Printf(TEXT("mode %s has no category the client knows"), *Mode.Id);
			return false;
		}
		Mode.HumanPlayersPerTeam = FMath::FloorToInt32(Team);
		Mode.bVersusAI = Matchmaking.Equals(TEXT("coop"), ESearchCase::CaseSensitive);
		Mode.bMatchmade = Mode.bVersusAI || Matchmaking.Equals(TEXT("casualSelect"), ESearchCase::CaseSensitive)
			|| Matchmaking.Equals(TEXT("draftPick"), ESearchCase::CaseSensitive);
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

FString BuildFluxSpellsBody(TConstArrayView<FString> Spells)
{
	FString Body;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
	Writer->WriteObjectStart();
	Writer->WriteArrayStart(TEXT("fluxSpells"));
	for (const FString& Spell : Spells)
	{
		Writer->WriteValue(Spell);
	}
	Writer->WriteArrayEnd();
	Writer->WriteObjectEnd();
	Writer->Close();
	return Body;
}

FString BuildTradeBody(int32 Seat)
{
	FString Body;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("seat"), Seat);
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

namespace
{
	/** A JSON object writer's output, condensed, as every request body is written. */
	template <typename FWriteFields>
	FString WriteBody(FWriteFields&& WriteFields)
	{
		FString Body;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
		Writer->WriteObjectStart();
		WriteFields(*Writer);
		Writer->WriteObjectEnd();
		Writer->Close();
		return Body;
	}

	/** {"id", "displayName"}: an account as the social routes name it. */
	bool ParseAccountObject(const FJsonObject& Object, FAccount& Out)
	{
		return StringField(Object, TEXT("id"), IdPattern, Out.Id) && StringField(Object, TEXT("displayName"), Out.DisplayName) && !Out.DisplayName.IsEmpty();
	}

	/** An array field of accounts. */
	bool AccountArrayField(const FJsonObject& Object, FStringView Name, TArray<FAccount>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object.HasTypedField<EJson::Array>(Name) || !Object.TryGetArrayField(Name, Values))
		{
			return false;
		}
		Out.Reset();
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			const TSharedPtr<FJsonObject>* Entry = nullptr;
			FAccount Account;
			if (!Value.IsValid() || !Value->TryGetObject(Entry) || !Entry->IsValid() || !ParseAccountObject(**Entry, Account))
			{
				return false;
			}
			Out.Add(MoveTemp(Account));
		}
		return true;
	}

	/** A number field that is a whole number, at least Min. */
	bool WholeField(const FJsonObject& Object, FStringView Name, int32 Min, int32& Out)
	{
		double Value = 0.0;
		if (!DurationField(Object, Name, Value) || Value != FMath::FloorToDouble(Value) || Value < Min || Value > MAX_int32)
		{
			return false;
		}
		Out = static_cast<int32>(Value);
		return true;
	}

	/** A lobby seat; its fields fit its kind. */
	bool ParseLobbySeat(const FJsonObject& Object, FLobbySeat& Out)
	{
		FString Kind;
		if (!StringField(Object, TEXT("side"), SidePattern, Out.Side) || !WholeField(Object, TEXT("index"), 0, Out.Index) || !StringField(Object, TEXT("kind"), Kind)
			|| !StringField(Object, TEXT("accountId"), Out.AccountId) || !StringField(Object, TEXT("displayName"), Out.DisplayName)
			|| !BoolField(Object, TEXT("host"), Out.bHost) || !StringField(Object, TEXT("vanguardId"), Out.VanguardId)
			|| !StringField(Object, TEXT("difficulty"), Out.Difficulty))
		{
			return false;
		}
		// The fields that do not apply to a seat's kind are empty.
		if (Kind.Equals(TEXT("human"), ESearchCase::CaseSensitive))
		{
			Out.Kind = ELobbySeatKind::Human;
			return MatchesWhole(IdPattern, Out.AccountId) && !Out.DisplayName.IsEmpty() && Out.VanguardId.IsEmpty() && Out.Difficulty.IsEmpty();
		}
		if (Kind.Equals(TEXT("bot"), ESearchCase::CaseSensitive))
		{
			Out.Kind = ELobbySeatKind::Bot;
			return Out.AccountId.IsEmpty() && !Out.bHost && MatchesWhole(ContentIdPattern, Out.VanguardId) && MatchesWhole(WordPattern, Out.Difficulty);
		}
		Out.Kind = ELobbySeatKind::Empty;
		return Kind.Equals(TEXT("empty"), ESearchCase::CaseSensitive) && Out.AccountId.IsEmpty() && Out.DisplayName.IsEmpty() && !Out.bHost
			&& Out.VanguardId.IsEmpty() && Out.Difficulty.IsEmpty();
	}
}

FString AccountLookupPath(const FString& DisplayName)
{
	return TEXT("/v1/accounts?displayName=") + FGenericPlatformHttp::UrlEncode(DisplayName);
}

bool ParseAccount(const FString& Body, FAccount& Out, FString& OutProblem)
{
	const TSharedPtr<FJsonObject> Root = ParseObject(Body);
	FAccount Account;
	if (!Root.IsValid() || !ParseAccountObject(*Root, Account))
	{
		OutProblem = TEXT("the account's ID or name is missing or not in the expected format");
		return false;
	}
	Out = MoveTemp(Account);
	return true;
}

bool ParseFriends(const FString& Body, FFriends& Out, FString& OutProblem)
{
	const TSharedPtr<FJsonObject> Root = ParseObject(Body);
	FFriends Friends;
	if (!Root.IsValid() || !AccountArrayField(*Root, TEXT("friends"), Friends.Friends) || !AccountArrayField(*Root, TEXT("incomingRequests"), Friends.Incoming)
		|| !AccountArrayField(*Root, TEXT("outgoingRequests"), Friends.Outgoing))
	{
		OutProblem = TEXT("the friends, or the requests to or from the player, are missing or not in the expected format");
		return false;
	}
	Out = MoveTemp(Friends);
	return true;
}

bool ParseFriendRequestOutcome(const FString& Body, FString& OutOutcome, FString& OutProblem)
{
	const TSharedPtr<FJsonObject> Root = ParseObject(Body);
	FString Outcome;
	if (!Root.IsValid() || !StringField(*Root, TEXT("outcome"), Outcome)
		|| !(Outcome.Equals(TEXT("requested"), ESearchCase::CaseSensitive) || Outcome.Equals(TEXT("friends"), ESearchCase::CaseSensitive)))
	{
		OutProblem = TEXT("the friend request's outcome is missing or not one the game knows");
		return false;
	}
	OutOutcome = MoveTemp(Outcome);
	return true;
}

FString BuildAccountBody(const FString& AccountId)
{
	return WriteBody([&AccountId](TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>& Writer) { Writer.WriteValue(TEXT("accountId"), AccountId); });
}

const FLobbySeat* FLobby::FindMember(const FString& AccountId) const
{
	return Seats.FindByPredicate([&AccountId](const FLobbySeat& Seat) { return Seat.Kind == ELobbySeatKind::Human && Seat.AccountId == AccountId; });
}

bool ParseLobby(const FString& Body, TOptional<FLobby>& OutLobby, FString& OutProblem)
{
	const TSharedPtr<FJsonObject> Root = ParseObject(Body);
	if (!Root.IsValid() || !Root->HasField(TEXT("lobby")))
	{
		OutProblem = TEXT("the answer has no \"lobby\"");
		return false;
	}
	if (Root->HasTypedField<EJson::Null>(TEXT("lobby")))
	{
		OutLobby.Reset();
		return true;
	}
	const FJsonObject* Object = ObjectField(*Root, TEXT("lobby"));
	const FJsonObject* Settings = Object ? ObjectField(*Object, TEXT("settings")) : nullptr;
	const FJsonObject* GoldRange = Object ? ObjectField(*Object, TEXT("startingGoldRange")) : nullptr;
	FLobby Lobby;
	FString Status;
	const TArray<TSharedPtr<FJsonValue>>* Seats = nullptr;
	if (!Object || !Settings || !GoldRange || !StringField(*Object, TEXT("id"), IdPattern, Lobby.Id)
		|| !StringField(*Object, TEXT("hostAccountId"), IdPattern, Lobby.HostAccountId) || !StringField(*Object, TEXT("status"), Status)
		|| !(Status.Equals(TEXT("open"), ESearchCase::CaseSensitive) || Status.Equals(TEXT("selecting"), ESearchCase::CaseSensitive))
		|| !WholeField(*Object, TEXT("playersPerSide"), 1, Lobby.PlayersPerSide) || !BoolField(*Settings, TEXT("victoryEnabled"), Lobby.bVictoryEnabled)
		|| !DurationField(*GoldRange, TEXT("min"), Lobby.StartingGoldMin) || !DurationField(*GoldRange, TEXT("max"), Lobby.StartingGoldMax)
		|| Lobby.StartingGoldMin > Lobby.StartingGoldMax || !Object->HasTypedField<EJson::Array>(TEXT("seats"))
		|| !Object->TryGetArrayField(TEXT("seats"), Seats) || !StringArrayField(*Object, TEXT("botVanguards"), ContentIdPattern, Lobby.BotVanguards)
		|| !StringArrayField(*Object, TEXT("botDifficulties"), WordPattern, Lobby.BotDifficulties))
	{
		OutProblem = TEXT("the lobby's ID, host, status, size, rules, seats or bot choices are missing or not in the expected format");
		return false;
	}
	Lobby.bSelecting = Status.Equals(TEXT("selecting"), ESearchCase::CaseSensitive);
	// Starting Gold is null for the game's own.
	double Gold = 0.0;
	if (DurationField(*Settings, TEXT("startingGold"), Gold))
	{
		Lobby.StartingGold = Gold;
	}
	else if (!Settings->HasTypedField<EJson::Null>(TEXT("startingGold")))
	{
		OutProblem = TEXT("the lobby's starting Gold is not a number or null");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : *Seats)
	{
		const TSharedPtr<FJsonObject>* SeatObject = nullptr;
		FLobbySeat Seat;
		if (!Value.IsValid() || !Value->TryGetObject(SeatObject) || !SeatObject->IsValid() || !ParseLobbySeat(**SeatObject, Seat))
		{
			OutProblem = TEXT("a seat of the lobby is not in the expected format");
			return false;
		}
		Lobby.Seats.Add(MoveTemp(Seat));
	}
	// Every seat of both sides, in order, and one host among the humans.
	bool bInOrder = Lobby.Seats.Num() == 2 * Lobby.PlayersPerSide;
	for (int32 Index = 0; bInOrder && Index < Lobby.Seats.Num(); ++Index)
	{
		const FLobbySeat& Seat = Lobby.Seats[Index];
		bInOrder = Seat.Side == (Index < Lobby.PlayersPerSide ? TEXT("A") : TEXT("B")) && Seat.Index == Index % Lobby.PlayersPerSide;
	}
	const FLobbySeat* Host = Lobby.FindMember(Lobby.HostAccountId);
	const int32 Hosts = Lobby.Seats.FilterByPredicate([](const FLobbySeat& Seat) { return Seat.bHost; }).Num();
	if (!bInOrder || !Host || !Host->bHost || Hosts != 1)
	{
		OutProblem = TEXT("the lobby's seats are not both sides' in order, or it does not seat exactly one host");
		return false;
	}
	OutLobby = MoveTemp(Lobby);
	return true;
}

bool ParseLobbyInvites(const FString& Body, TArray<FLobbyInvite>& Out, FString& OutProblem)
{
	const TSharedPtr<FJsonObject> Root = ParseObject(Body);
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Root.IsValid() || !Root->HasTypedField<EJson::Array>(TEXT("invites")) || !Root->TryGetArrayField(TEXT("invites"), Values))
	{
		OutProblem = TEXT("the answer has no list of invitations");
		return false;
	}
	TArray<FLobbyInvite> Invites;
	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		const TSharedPtr<FJsonObject>* Entry = nullptr;
		FLobbyInvite Invite;
		const FJsonObject* Inviter = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(Entry) || !Entry->IsValid() || !StringField(**Entry, TEXT("id"), IdPattern, Invite.Id)
			|| !StringField(**Entry, TEXT("lobbyId"), IdPattern, Invite.LobbyId) || (Inviter = ObjectField(**Entry, TEXT("inviter"))) == nullptr
			|| !ParseAccountObject(*Inviter, Invite.Inviter))
		{
			OutProblem = TEXT("an invitation is not in the expected format");
			return false;
		}
		Invites.Add(MoveTemp(Invite));
	}
	Out = MoveTemp(Invites);
	return true;
}

FString LobbyBotPath(const FString& Side, int32 Index)
{
	return FString::Printf(TEXT("/v1/lobby/seats/%s/%d/bot"), *Side, Index);
}

FString BuildSeatBody(const FString& Side, int32 Index)
{
	return WriteBody([&Side, Index](TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>& Writer) {
		Writer.WriteValue(TEXT("side"), Side);
		Writer.WriteValue(TEXT("index"), Index);
	});
}

FString BuildBotBody(const FString& VanguardId, const FString& Difficulty)
{
	return WriteBody([&VanguardId, &Difficulty](TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>& Writer) {
		Writer.WriteValue(TEXT("vanguardId"), VanguardId);
		Writer.WriteValue(TEXT("difficulty"), Difficulty);
	});
}

FString BuildLobbySettingsBody(bool bVictoryEnabled, TOptional<double> StartingGold)
{
	return WriteBody([bVictoryEnabled, &StartingGold](TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>& Writer) {
		Writer.WriteValue(TEXT("victoryEnabled"), bVictoryEnabled);
		if (StartingGold.IsSet())
		{
			Writer.WriteValue(TEXT("startingGold"), StartingGold.GetValue());
		}
		else
		{
			Writer.WriteNull(TEXT("startingGold"));
		}
	});
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
		Writer->WriteValue(TEXT("personalLoss"), Participant.bPersonalLoss);
		Writer->WriteValue(TEXT("absentSeconds"), Participant.AbsentSeconds);
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();
	// The scoreboard (ADR-017 §5): every player, a bot with no account. With nobody recorded, as when a
	// match is abandoned before it prepares anyone, there is none: null, never two empty teams.
	if (Result.Players.IsEmpty())
	{
		Writer->WriteNull(TEXT("players"));
	}
	else
	{
		Writer->WriteArrayStart(TEXT("players"));
		for (const FVeyraPlayerResult& Player : Result.Players)
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("side"), Player.Side == EVeyraTeam::A ? TEXT("A") : TEXT("B"));
			Writer->WriteValue(TEXT("name"), Player.DisplayName);
			if (Player.AccountId.IsEmpty())
			{
				Writer->WriteNull(TEXT("accountId"));
			}
			else
			{
				Writer->WriteValue(TEXT("accountId"), Player.AccountId);
			}
			Writer->WriteValue(TEXT("vanguardId"), Player.VanguardId.ToString());
			WriteStatistics(*Writer, Player.Statistics);
			WriteSlots(*Writer, TEXT("items"), Player.Statistics.Items, 0);
			WriteSlots(*Writer, TEXT("fluxSpells"), Player.Statistics.FluxSpells, SpellSlotCount);
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();
	}
	// Each Flux Well secured, once per capture (Match Statistics Bible §5).
	Writer->WriteArrayStart(TEXT("wells"));
	for (const FVeyraWellCapture& Capture : Result.Wells)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("site"), Capture.Site);
		Writer->WriteValue(TEXT("side"), Capture.Side == EVeyraTeam::A ? TEXT("A") : TEXT("B"));
		Writer->WriteValue(TEXT("atSeconds"), Capture.AtSeconds);
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
