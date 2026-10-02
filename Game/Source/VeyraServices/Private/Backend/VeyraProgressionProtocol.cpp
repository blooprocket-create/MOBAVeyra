// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Backend/VeyraProgressionProtocol.h"

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
	/** The content ID format the game's tuning uses (Game/Tuning/README.md). */
	const TCHAR* const ProgressionContentIdPattern = TEXT("^[a-z][a-z0-9]*(_[a-z0-9]+)*$");
	/** A backend code: an entitlement's source, or a reward's reason. */
	const TCHAR* const ProgressionWordPattern = TEXT("^[a-z_]{1,64}$");
	/** The largest whole number a JSON double carries exactly: 2^53. */
	constexpr double MaxExactWhole = 9007199254740992.0;

	bool ProgressionMatches(const TCHAR* Pattern, const FString& Text)
	{
		FRegexMatcher Matcher(FRegexPattern(FString(Pattern)), Text);
		return Matcher.FindNext() && Matcher.GetMatchBeginning() == 0 && Matcher.GetMatchEnding() == Text.Len();
	}

	TSharedPtr<FJsonObject> ProgressionParseBody(const FString& Body)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Body);
		return FJsonSerializer::Deserialize(Reader, Root) ? Root : nullptr;
	}

	const FJsonObject* ProgressionChild(const FJsonObject& Object, const TCHAR* Name)
	{
		const TSharedPtr<FJsonObject>* Child = nullptr;
		return Object.HasTypedField<EJson::Object>(Name) && Object.TryGetObjectField(Name, Child) && Child->IsValid() ? Child->Get() : nullptr;
	}

	/** A whole, non-negative amount. */
	bool ProgressionAmount(const FJsonObject& Object, const TCHAR* Name, int64& Out)
	{
		double Value = 0.0;
		if (!Object.HasTypedField<EJson::Number>(Name) || !Object.TryGetNumberField(Name, Value) || !FMath::IsFinite(Value) || Value < 0.0
			|| Value != FMath::FloorToDouble(Value) || Value > MaxExactWhole)
		{
			return false;
		}
		Out = static_cast<int64>(Value);
		return true;
	}

	/** A whole level or tier, at least Min. */
	bool ProgressionLevel(const FJsonObject& Object, const TCHAR* Name, int32 Min, int32& Out)
	{
		int64 Value = 0;
		if (!ProgressionAmount(Object, Name, Value) || Value < Min || Value > MAX_int32)
		{
			return false;
		}
		Out = static_cast<int32>(Value);
		return true;
	}

	bool ProgressionBool(const FJsonObject& Object, const TCHAR* Name, bool& Out)
	{
		return Object.HasTypedField<EJson::Boolean>(Name) && Object.TryGetBoolField(Name, Out);
	}

	/** A field that is null, which leaves Out empty, or a string in Pattern's format. */
	bool ProgressionNullableWord(const FJsonObject& Object, const TCHAR* Name, const TCHAR* Pattern, FString& Out)
	{
		if (Object.HasTypedField<EJson::Null>(Name))
		{
			Out.Reset();
			return true;
		}
		return Object.HasTypedField<EJson::String>(Name) && Object.TryGetStringField(Name, Out) && ProgressionMatches(Pattern, Out);
	}

	bool ReadProgressionObject(const FJsonObject& Object, FProgression& Out)
	{
		return ProgressionLevel(Object, TEXT("level"), 1, Out.Level) && ProgressionAmount(Object, TEXT("levelXp"), Out.LevelXP)
			&& ProgressionAmount(Object, TEXT("levelNeed"), Out.LevelNeed) && ProgressionAmount(Object, TEXT("lifetimeXp"), Out.LifetimeXP)
			&& ProgressionAmount(Object, TEXT("flux"), Out.Flux) && ProgressionAmount(Object, TEXT("refinedFlux"), Out.RefinedFlux);
	}

	bool ReadCollectionEntry(const FJsonObject& Object, FCollectionEntry& Out)
	{
		const FJsonObject* Price = ProgressionChild(Object, TEXT("price"));
		const FJsonObject* Mastery = ProgressionChild(Object, TEXT("mastery"));
		return Price && Mastery && Object.TryGetStringField(TEXT("vanguardId"), Out.VanguardId) && ProgressionMatches(ProgressionContentIdPattern, Out.VanguardId)
			&& ProgressionBool(Object, TEXT("owned"), Out.bOwned) && ProgressionNullableWord(Object, TEXT("source"), ProgressionWordPattern, Out.Source)
			&& ProgressionBool(Object, TEXT("rotation"), Out.bRotation) && ProgressionBool(Object, TEXT("purchasable"), Out.bPurchasable)
			&& ProgressionAmount(*Price, TEXT("flux"), Out.PriceFlux) && ProgressionAmount(*Price, TEXT("refinedFlux"), Out.PriceRefinedFlux)
			&& ProgressionLevel(*Mastery, TEXT("level"), 1, Out.Mastery.Level) && ProgressionAmount(*Mastery, TEXT("levelPoints"), Out.Mastery.LevelPoints)
			&& ProgressionAmount(*Mastery, TEXT("levelNeed"), Out.Mastery.LevelNeed) && ProgressionAmount(*Mastery, TEXT("lifetimePoints"), Out.Mastery.LifetimePoints)
			&& ProgressionLevel(*Mastery, TEXT("emoteTier"), 0, Out.Mastery.EmoteTier);
	}
}

const TCHAR* CurrencyName(ECurrency Currency)
{
	return Currency == ECurrency::RefinedFlux ? TEXT("refinedFlux") : TEXT("flux");
}

bool ParseProgression(const FString& Body, FProgression& Out, FString& OutProblem)
{
	const TSharedPtr<FJsonObject> Root = ProgressionParseBody(Body);
	const FJsonObject* Object = Root.IsValid() ? ProgressionChild(*Root, TEXT("progression")) : nullptr;
	FProgression Progression;
	if (!Object || !ReadProgressionObject(*Object, Progression))
	{
		OutProblem = TEXT("the progression's level, XP or balances are missing or not whole amounts");
		return false;
	}
	Out = Progression;
	return true;
}

bool ParseCollection(const FString& Body, TArray<FCollectionEntry>& Out, FString& OutProblem)
{
	const TSharedPtr<FJsonObject> Root = ProgressionParseBody(Body);
	const TArray<TSharedPtr<FJsonValue>>* Vanguards = nullptr;
	if (!Root.IsValid() || !Root->HasTypedField<EJson::Array>(TEXT("vanguards")) || !Root->TryGetArrayField(TEXT("vanguards"), Vanguards))
	{
		OutProblem = TEXT("the answer does not list the Collection's Vanguards");
		return false;
	}
	TArray<FCollectionEntry> Entries;
	for (const TSharedPtr<FJsonValue>& Value : *Vanguards)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(Object) || !Object->IsValid() || !ReadCollectionEntry(**Object, Entries.AddDefaulted_GetRef()))
		{
			OutProblem = TEXT("a Collection entry's Vanguard, ownership, price or Mastery is not in the expected format");
			return false;
		}
	}
	Out = MoveTemp(Entries);
	return true;
}

bool ParsePurchase(const FString& Body, FProgression& Out, FString& OutProblem)
{
	const TSharedPtr<FJsonObject> Root = ProgressionParseBody(Body);
	const FJsonObject* Purchase = Root.IsValid() ? ProgressionChild(*Root, TEXT("purchase")) : nullptr;
	const FJsonObject* Object = Root.IsValid() ? ProgressionChild(*Root, TEXT("progression")) : nullptr;
	FProgression Progression;
	if (!Purchase || !Object || !ReadProgressionObject(*Object, Progression))
	{
		OutProblem = TEXT("the purchase or the progression after it is missing or not in the expected format");
		return false;
	}
	Out = Progression;
	return true;
}

bool ParseMatchRewards(const FJsonObject& Object, FMatchRewards& Out)
{
	FMatchRewards Rewards;
	if (!ProgressionNullableWord(Object, TEXT("reason"), ProgressionWordPattern, Rewards.Reason) || !ProgressionAmount(Object, TEXT("accountXp"), Rewards.AccountXP)
		|| !ProgressionLevel(Object, TEXT("levelBefore"), 1, Rewards.LevelBefore) || !ProgressionLevel(Object, TEXT("levelAfter"), Rewards.LevelBefore, Rewards.LevelAfter)
		|| !ProgressionAmount(Object, TEXT("flux"), Rewards.Flux) || !ProgressionAmount(Object, TEXT("refinedFlux"), Rewards.RefinedFlux)
		|| !ProgressionNullableWord(Object, TEXT("vanguardId"), ProgressionContentIdPattern, Rewards.VanguardId)
		|| !ProgressionAmount(Object, TEXT("masteryPoints"), Rewards.MasteryPoints) || !ProgressionLevel(Object, TEXT("masteryBefore"), 0, Rewards.MasteryBefore)
		|| !ProgressionLevel(Object, TEXT("masteryAfter"), Rewards.MasteryBefore, Rewards.MasteryAfter))
	{
		return false;
	}
	Out = MoveTemp(Rewards);
	return true;
}

FString BuildPurchaseBody(const FString& PurchaseId, const FString& VanguardId, ECurrency Currency)
{
	FString Body;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("purchaseId"), PurchaseId);
	Writer->WriteValue(TEXT("vanguardId"), VanguardId);
	Writer->WriteValue(TEXT("currency"), FString(CurrencyName(Currency)));
	Writer->WriteObjectEnd();
	Writer->Close();
	return Body;
}
}
