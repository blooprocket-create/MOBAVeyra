// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Backend/VeyraConductProtocol.h"

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
	/** A report reason's format on the wire. */
	const TCHAR* const ConductReasonPattern = TEXT("^[a-z_]{1,64}$");

	bool ConductIsReason(const FString& Text)
	{
		FRegexMatcher Matcher(FRegexPattern(FString(ConductReasonPattern)), Text);
		return Matcher.FindNext() && Matcher.GetMatchBeginning() == 0 && Matcher.GetMatchEnding() == Text.Len();
	}

	/** A field holding an array of non-empty strings, each passing Accept. */
	bool ConductStrings(const FJsonObject& Object, const TCHAR* Name, TFunctionRef<bool(const FString&)> Accept, TArray<FString>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object.HasTypedField<EJson::Array>(Name) || !Object.TryGetArrayField(Name, Values))
		{
			return false;
		}
		TArray<FString> Strings;
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			FString Text;
			if (!Value.IsValid() || !Value->TryGetString(Text) || Text.IsEmpty() || !Accept(Text))
			{
				return false;
			}
			Strings.Add(MoveTemp(Text));
		}
		Out = MoveTemp(Strings);
		return true;
	}

	bool ConductPlayers(const FJsonObject& Object, TArray<FConductPlayer>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object.HasTypedField<EJson::Array>(TEXT("players")) || !Object.TryGetArrayField(TEXT("players"), Values))
		{
			return false;
		}
		TArray<FConductPlayer> Players;
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			const TSharedPtr<FJsonObject>* Player = nullptr;
			FConductPlayer& Read = Players.AddDefaulted_GetRef();
			if (!Value.IsValid() || !Value->TryGetObject(Player) || !Player->IsValid() || !(*Player)->TryGetStringField(TEXT("name"), Read.Name)
				|| Read.Name.IsEmpty() || !(*Player)->HasTypedField<EJson::Boolean>(TEXT("teammate")) || !(*Player)->TryGetBoolField(TEXT("teammate"), Read.bTeammate))
			{
				return false;
			}
		}
		Out = MoveTemp(Players);
		return true;
	}

	FString ConductWrite(TFunctionRef<void(TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>&)> Fill)
	{
		FString Body;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
		Writer->WriteObjectStart();
		Fill(*Writer);
		Writer->WriteObjectEnd();
		Writer->Close();
		return Body;
	}
}

bool ParseConductRecord(const FString& Body, FConductRecord& Out, FString& OutProblem)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedPtr<FJsonObject>* Object = nullptr;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(Body), Root) || !Root.IsValid() || !Root->HasTypedField<EJson::Object>(TEXT("conduct"))
		|| !Root->TryGetObjectField(TEXT("conduct"), Object) || !Object->IsValid())
	{
		OutProblem = TEXT("the answer has no conduct record");
		return false;
	}
	const FJsonObject& Conduct = **Object;
	FConductRecord Record;
	double DetailsMax = 0.0;
	const bool bCommended = Conduct.HasTypedField<EJson::Null>(TEXT("commended"))
		|| (Conduct.HasTypedField<EJson::String>(TEXT("commended")) && Conduct.TryGetStringField(TEXT("commended"), Record.Commended) && !Record.Commended.IsEmpty());
	if (!bCommended || !ConductPlayers(Conduct, Record.Players) || !ConductStrings(Conduct, TEXT("reported"), [](const FString&) { return true; }, Record.Reported)
		|| !ConductStrings(Conduct, TEXT("reasons"), ConductIsReason, Record.Reasons) || Record.Reasons.IsEmpty()
		|| !Conduct.HasTypedField<EJson::Number>(TEXT("detailsMaxCharacters")) || !Conduct.TryGetNumberField(TEXT("detailsMaxCharacters"), DetailsMax)
		|| DetailsMax < 0.0 || DetailsMax > MAX_int32 || DetailsMax != FMath::FloorToDouble(DetailsMax))
	{
		OutProblem = TEXT("the conduct record's players, names, reasons or details limit are missing or not in the expected format");
		return false;
	}
	Record.DetailsMaxCharacters = static_cast<int32>(DetailsMax);
	Out = MoveTemp(Record);
	return true;
}

FString BuildReportBody(const FString& ReportedName, const FString& Reason, const FString& Details, const FString& ClientId)
{
	return ConductWrite([&](TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>& Writer) {
		Writer.WriteValue(TEXT("reportedName"), ReportedName);
		Writer.WriteValue(TEXT("reason"), Reason);
		Writer.WriteValue(TEXT("details"), Details);
		Writer.WriteValue(TEXT("clientId"), ClientId);
	});
}

FString BuildCommendationBody(const FString& Name)
{
	return ConductWrite([&](TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>& Writer) { Writer.WriteValue(TEXT("name"), Name); });
}
}
