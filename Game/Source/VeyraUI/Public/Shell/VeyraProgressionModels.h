// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Backend/VeyraProgressionProtocol.h"
#include "Client/VeyraClientFlowTypes.h"
#include "Internationalization/Text.h"

/** The top bar's account readout (ADR-045 §8): the Account Level, its XP and the account currencies. */
struct FVeyraProgressionBarModel
{
	/** Unset progression, before its first read or from a backend without it, shows nothing. */
	bool bShown = false;
	FText Level;
	FText XP;
	/** How far the level is toward the next, from 0 to 1. */
	float Fraction = 0.0f;
	/** Flux and Refined Flux, the persistent account currencies, never Team Flux. */
	FText Currencies;
};

/** One released Vanguard on the Collection page (Account, Collection & Mastery Bible §4). */
struct FVeyraCollectionCard
{
	FString VanguardId;
	FText Name;
	/** Owned, lent by the rotation, or neither. */
	FText Status;
	FText MasteryShort;
	/** The opened card's detail: Mastery Level and points toward the next, lifetime points, emote tier. */
	TArray<FText> Details;
	bool bPurchasable = false;
	/** Whether each currency's Buy is offered now: purchasable, the intent allowed, and the balance covering it as last read. */
	bool bCanBuyWithFlux = false;
	bool bCanBuyWithRefinedFlux = false;
	int64 PriceFlux = 0;
	int64 PriceRefinedFlux = 0;
	/** What the roster's search and tabs read (ADR-058 §2–§3). */
	bool bOwned = false;
	bool bRotation = false;
	bool bFavorite = false;
};

struct FVeyraCollectionModel
{
	bool bLoaded = false;
	/** What came of the last purchase or favorite; empty for none. */
	FText Feedback;
	/** The balances to spend, or empty before they are read. */
	FText Balance;
	/** Every released Vanguard, in the catalog's order. */
	TArray<FVeyraCollectionCard> Cards;
};

/** What the results screen says the match gave the player (ADR-045 §8), apart from the match's Gold, XP and Team Flux. */
struct FVeyraRewardsModel
{
	/** Unset before a result, or from a backend without progression. */
	bool bShown = false;
	TArray<FText> Lines;
};

namespace VeyraProgressionModels
{
	VEYRAUI_API FVeyraProgressionBarModel DescribeBar(const TOptional<VeyraBackendProtocol::FProgression>& Progression);

	/** The Collection as read; bCanPurchase is whether the purchase intent is allowed now. */
	VEYRAUI_API FVeyraCollectionModel DescribeCollection(const FVeyraClientSnapshot& Snapshot, bool bCanPurchase);

	/** A result's rewards; Wait says whether ones it came without are still to come. */
	VEYRAUI_API FVeyraRewardsModel DescribeRewards(const TOptional<VeyraBackendProtocol::FMatchOutcome>& Result, EVeyraRewardsWait Wait = EVeyraRewardsWait::None);

	/** "Collection Bryn": the button that opens a Vanguard's card. */
	VEYRAUI_API FText CollectionCardLabel(const FString& VanguardId);

	/** "Buy Bryn for 3,000 Flux": a Buy action, before its confirmation. */
	VEYRAUI_API FText BuyLabel(const FString& VanguardId, VeyraBackendProtocol::ECurrency Currency, int64 Price);

	/** "Add Bryn to Favorites", or "Remove Bryn from Favorites" for a favorite: an opened card's toggle (ADR-058 §3). */
	VEYRAUI_API FText FavoriteLabel(const FString& VanguardId, bool bFavorite);

	/** "Confirm Buy Bryn for 3,000 Flux": the confirmation's own action. */
	VEYRAUI_API FText ConfirmBuyLabel(const FString& VanguardId, VeyraBackendProtocol::ECurrency Currency, int64 Price);

	/** The confirmation's question, naming the price, the currency and the balance (Bible §7). */
	VEYRAUI_API FText ConfirmBuyPrompt(const FString& VanguardId, VeyraBackendProtocol::ECurrency Currency, int64 Price,
		const TOptional<VeyraBackendProtocol::FProgression>& Progression);

	/** An amount in a currency: "3,000 Flux" or "550 Refined Flux". */
	VEYRAUI_API FText Amount(VeyraBackendProtocol::ECurrency Currency, int64 Value);

	/** Progression, the Collection and rewards, for the shell's rebuild signature. */
	VEYRAUI_API FString Signature(const FVeyraClientSnapshot& Snapshot);
}
