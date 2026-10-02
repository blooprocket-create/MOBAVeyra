// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"

class FJsonObject;

/**
 * Account progression on the wire (ADR-045 §7): the account's level and currencies, its Collection, a
 * purchase, and what a match gave the player. Flux and Refined Flux here are persistent account
 * currencies, never in-match Team Flux. Every amount is the trusted backend's; the client only shows them.
 */
namespace VeyraBackendProtocol
{
	/** An account currency a purchase spends (Account, Collection & Mastery Bible §3). */
	enum class ECurrency : uint8
	{
		/** Earned at every level-up. */
		Flux,
		/** The premium currency; earned at milestones. */
		RefinedFlux,
	};

	/** The currency's name on the wire: "flux" or "refinedFlux". */
	VEYRASERVICES_API const TCHAR* CurrencyName(ECurrency Currency);

	/** The account's level and balances, as GET /v1/me/progression reports them. */
	struct FProgression
	{
		int32 Level = 1;
		/** XP into the current level, toward LevelNeed. */
		int64 LevelXP = 0;
		/** What the current level takes to the next. */
		int64 LevelNeed = 0;
		int64 LifetimeXP = 0;
		int64 Flux = 0;
		int64 RefinedFlux = 0;
	};

	/** The account's Mastery of one Vanguard (Bible §5). */
	struct FMastery
	{
		int32 Level = 1;
		int64 LevelPoints = 0;
		int64 LevelNeed = 0;
		int64 LifetimePoints = 0;
		/** How many of the mastery emote's appearances the level has reached. */
		int32 EmoteTier = 0;
	};

	/** One released Vanguard in the player's Collection (Bible §4): shown whatever the player owns. */
	struct FCollectionEntry
	{
		FString VanguardId;
		bool bOwned = false;
		/** How an owned Vanguard was gained: "starter" or "purchase"; empty when not owned. */
		FString Source;
		/** Whether this week's rotation lends it. */
		bool bRotation = false;
		int64 PriceFlux = 0;
		int64 PriceRefinedFlux = 0;
		/** Whether the player can buy it now. */
		bool bPurchasable = false;
		FMastery Mastery;
	};

	/** What one match gave the player (ADR-045 §7): account rewards and Mastery, apart from the match's own Gold and XP. */
	struct FMatchRewards
	{
		/** Why nothing was earned, such as "custom", "no_contest" or "coop_level"; empty when it was. */
		FString Reason;
		int64 AccountXP = 0;
		int32 LevelBefore = 1;
		int32 LevelAfter = 1;
		int64 Flux = 0;
		int64 RefinedFlux = 0;
		/** The Vanguard the player played; empty for an old match without one. */
		FString VanguardId;
		int64 MasteryPoints = 0;
		int32 MasteryBefore = 0;
		int32 MasteryAfter = 0;
	};

	/** Reads the answer to GET /v1/me/progression. False, with the problem, if it is not one. */
	VEYRASERVICES_API bool ParseProgression(const FString& Body, FProgression& Out, FString& OutProblem);

	/** Reads the answer to GET /v1/me/collection: every released Vanguard in the catalog's order. */
	VEYRASERVICES_API bool ParseCollection(const FString& Body, TArray<FCollectionEntry>& Out, FString& OutProblem);

	/** Reads the answer to POST /v1/me/purchases: the account's progression after the purchase. */
	VEYRASERVICES_API bool ParsePurchase(const FString& Body, FProgression& Out, FString& OutProblem);

	/** Reads a match result's "rewards" object. */
	VEYRASERVICES_API bool ParseMatchRewards(const FJsonObject& Object, FMatchRewards& Out);

	/** The body of POST /v1/me/purchases. PurchaseId is the client's, so a retry never spends twice. */
	VEYRASERVICES_API FString BuildPurchaseBody(const FString& PurchaseId, const FString& VanguardId, ECurrency Currency);
}
