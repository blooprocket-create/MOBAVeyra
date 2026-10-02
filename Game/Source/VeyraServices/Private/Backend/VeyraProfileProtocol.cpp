// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Backend/VeyraProfileProtocol.h"

#include "Dom/JsonObject.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Internationalization/Regex.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace VeyraBackendProtocol
{
namespace
{
	/** A catalog entry's ID: an icon or a background. */
	const TCHAR* const ProfileEntryPattern = TEXT("^[a-z][a-z0-9_]{0,63}$");
	/** The content ID format the game's tuning uses (Game/Tuning/README.md). */
	const TCHAR* const ProfileVanguardPattern = TEXT("^[a-z][a-z0-9]*(_[a-z0-9]+)*$");

	bool ProfileMatches(const TCHAR* Pattern, const FString& Text)
	{
		FRegexMatcher Matcher(FRegexPattern(FString(Pattern)), Text);
		return Matcher.FindNext() && Matcher.GetMatchBeginning() == 0 && Matcher.GetMatchEnding() == Text.Len();
	}

	const FJsonObject* ProfileChild(const FJsonObject& Object, const TCHAR* Name)
	{
		const TSharedPtr<FJsonObject>* Child = nullptr;
		return Object.HasTypedField<EJson::Object>(Name) && Object.TryGetObjectField(Name, Child) && Child->IsValid() ? Child->Get() : nullptr;
	}

	bool ProfileEntry(const FJsonObject& Object, const TCHAR* Name, FString& Out)
	{
		return Object.HasTypedField<EJson::String>(Name) && Object.TryGetStringField(Name, Out) && ProfileMatches(ProfileEntryPattern, Out);
	}

	/** A whole number at least Min. */
	bool ProfileWhole(const FJsonObject& Object, const TCHAR* Name, int32 Min, int32& Out)
	{
		double Value = 0.0;
		if (!Object.HasTypedField<EJson::Number>(Name) || !Object.TryGetNumberField(Name, Value) || Value != FMath::FloorToDouble(Value) || Value < Min
			|| Value > MAX_int32)
		{
			return false;
		}
		Out = static_cast<int32>(Value);
		return true;
	}

	/** An array of strings in Pattern's format; empty only when bMayBeEmpty. */
	bool ProfileEntries(const FJsonObject& Object, const TCHAR* Name, TArray<FString>& Out, const TCHAR* Pattern = ProfileEntryPattern, bool bMayBeEmpty = false)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object.HasTypedField<EJson::Array>(Name) || !Object.TryGetArrayField(Name, Values) || (Values->IsEmpty() && !bMayBeEmpty))
		{
			return false;
		}
		TArray<FString> Entries;
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			FString Entry;
			if (!Value.IsValid() || !Value->TryGetString(Entry) || !ProfileMatches(Pattern, Entry))
			{
				return false;
			}
			Entries.Add(MoveTemp(Entry));
		}
		Out = MoveTemp(Entries);
		return true;
	}

	FString ProfileSegment(const FString& Name)
	{
		return FGenericPlatformHttp::UrlEncode(Name);
	}
}

FString ProfilePath(const FString& Name)
{
	return TEXT("/v1/profiles/") + ProfileSegment(Name);
}

FString ProfileMatchesPath(const FString& Name, const FString& Cursor)
{
	const FString Path = ProfilePath(Name) + TEXT("/matches");
	return Cursor.IsEmpty() ? Path : Path + TEXT("?cursor=") + FGenericPlatformHttp::UrlEncode(Cursor);
}

FString ProfileMatchPath(const FString& Name, const FString& MatchId)
{
	return ProfilePath(Name) + TEXT("/matches/") + ProfileSegment(MatchId);
}

bool ParsePublicProfile(const FString& Body, FPublicProfile& Out, FString& OutProblem)
{
	TSharedPtr<FJsonObject> Root;
	const FJsonObject* Object = FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(Body), Root) && Root.IsValid() ? ProfileChild(*Root, TEXT("profile")) : nullptr;
	FPublicProfile Profile;
	bool bValid = Object && Object->TryGetStringField(TEXT("name"), Profile.Name) && !Profile.Name.IsEmpty() && ProfileEntry(*Object, TEXT("icon"), Profile.Icon)
		&& ProfileEntry(*Object, TEXT("background"), Profile.Background) && ProfileWhole(*Object, TEXT("level"), 1, Profile.Level)
		&& Object->HasTypedField<EJson::Boolean>(TEXT("sharesMatchHistory")) && Object->TryGetBoolField(TEXT("sharesMatchHistory"), Profile.bSharesMatchHistory);
	if (bValid && !Object->HasTypedField<EJson::Null>(TEXT("featured")))
	{
		const FJsonObject* Featured = ProfileChild(*Object, TEXT("featured"));
		FProfileFeatured& Read = Profile.Featured.Emplace();
		bValid = Featured && Featured->TryGetStringField(TEXT("vanguardId"), Read.VanguardId) && ProfileMatches(ProfileVanguardPattern, Read.VanguardId)
			&& ProfileWhole(*Featured, TEXT("masteryLevel"), 1, Read.MasteryLevel);
	}
	if (!bValid)
	{
		OutProblem = TEXT("the profile's name, icon, background, level, featured Vanguard or sharing is missing or not in the expected format");
		return false;
	}
	Out = MoveTemp(Profile);
	return true;
}

bool ParseProfileSettings(const FString& Body, FProfileSettings& OutSettings, FProfileCatalog& OutCatalog, FString& OutProblem)
{
	TSharedPtr<FJsonObject> Root;
	const bool bRead = FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(Body), Root) && Root.IsValid();
	const FJsonObject* Object = bRead ? ProfileChild(*Root, TEXT("settings")) : nullptr;
	const FJsonObject* Catalog = bRead ? ProfileChild(*Root, TEXT("catalog")) : nullptr;
	FProfileSettings Settings;
	FProfileCatalog Read;
	bool bValid = Object && Catalog && ProfileEntry(*Object, TEXT("icon"), Settings.Icon) && ProfileEntry(*Object, TEXT("background"), Settings.Background)
		&& Object->HasTypedField<EJson::Boolean>(TEXT("showMatchHistory")) && Object->TryGetBoolField(TEXT("showMatchHistory"), Settings.bShowMatchHistory)
		&& ProfileEntries(*Catalog, TEXT("icons"), Read.Icons) && ProfileEntries(*Catalog, TEXT("backgrounds"), Read.Backgrounds)
		&& ProfileEntry(*Catalog, TEXT("defaultIcon"), Read.DefaultIcon) && ProfileEntry(*Catalog, TEXT("defaultBackground"), Read.DefaultBackground)
		&& ProfileEntries(*Catalog, TEXT("featuredChoices"), Read.FeaturedChoices, ProfileVanguardPattern, /*bMayBeEmpty*/ true);
	if (bValid && !Object->HasTypedField<EJson::Null>(TEXT("featuredVanguardId")))
	{
		bValid = Object->HasTypedField<EJson::String>(TEXT("featuredVanguardId")) && Object->TryGetStringField(TEXT("featuredVanguardId"), Settings.FeaturedVanguardId)
			&& ProfileMatches(ProfileVanguardPattern, Settings.FeaturedVanguardId);
	}
	if (!bValid)
	{
		OutProblem = TEXT("the profile settings or the catalog are missing or not in the expected format");
		return false;
	}
	OutSettings = MoveTemp(Settings);
	OutCatalog = MoveTemp(Read);
	return true;
}

bool ParseDisplayNameStatus(const FString& Body, FDisplayNameStatus& Out, FString& OutProblem)
{
	TSharedPtr<FJsonObject> Root;
	const FJsonObject* Object = FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(Body), Root) && Root.IsValid() ? ProfileChild(*Root, TEXT("displayName")) : nullptr;
	const FJsonObject* Price = Object ? ProfileChild(*Object, TEXT("price")) : nullptr;
	FDisplayNameStatus Status;
	double Flux = 0.0;
	double RefinedFlux = 0.0;
	bool bValid = Object && Price && Object->TryGetStringField(TEXT("name"), Status.Name) && !Status.Name.IsEmpty()
		&& Object->HasTypedField<EJson::Boolean>(TEXT("freeChangeAvailable")) && Object->TryGetBoolField(TEXT("freeChangeAvailable"), Status.bFreeChangeAvailable)
		&& Object->HasTypedField<EJson::Boolean>(TEXT("renameRequired")) && Object->TryGetBoolField(TEXT("renameRequired"), Status.bRenameRequired)
		&& Price->TryGetNumberField(TEXT("flux"), Flux) && Price->TryGetNumberField(TEXT("refinedFlux"), RefinedFlux) && Flux >= 0.0 && RefinedFlux >= 0.0
		&& Flux == FMath::FloorToDouble(Flux) && RefinedFlux == FMath::FloorToDouble(RefinedFlux);
	if (bValid && !Object->HasTypedField<EJson::Null>(TEXT("nextChangeAt")))
	{
		FString When;
		FDateTime At;
		bValid = Object->TryGetStringField(TEXT("nextChangeAt"), When) && FDateTime::ParseIso8601(*When, At);
		Status.NextChangeAt = At;
	}
	if (!bValid)
	{
		OutProblem = TEXT("the display name, its free change, its next change, its price or the required rename is missing or not in the expected format");
		return false;
	}
	Status.PriceFlux = static_cast<int64>(Flux);
	Status.PriceRefinedFlux = static_cast<int64>(RefinedFlux);
	Out = MoveTemp(Status);
	return true;
}

FString BuildDisplayNameBody(const FString& Name, const FString& Currency)
{
	FString Body;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("name"), Name);
	Writer->WriteValue(TEXT("currency"), Currency);
	Writer->WriteObjectEnd();
	Writer->Close();
	return Body;
}

FString BuildProfileSettingsBody(const FProfileSettings& Settings)
{
	FString Body;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("icon"), Settings.Icon);
	Writer->WriteValue(TEXT("background"), Settings.Background);
	if (Settings.FeaturedVanguardId.IsEmpty())
	{
		Writer->WriteNull(TEXT("featuredVanguardId"));
	}
	else
	{
		Writer->WriteValue(TEXT("featuredVanguardId"), Settings.FeaturedVanguardId);
	}
	Writer->WriteValue(TEXT("showMatchHistory"), Settings.bShowMatchHistory);
	Writer->WriteObjectEnd();
	Writer->Close();
	return Body;
}
}
