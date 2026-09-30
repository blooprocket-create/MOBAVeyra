// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Buyback/VeyraBuybackRules.h"
#include "Content/VeyraContentId.h"
#include "Internationalization/Text.h"
#include "Inventory/VeyraInventoryRules.h"
#include "Tools/VeyraVisionToolComponent.h"
#include "Tuning/VeyraItemsTuning.h"

class AActor;
struct FVeyraItemStatsTuning;

/** One item the shop sells, as the participant would pay for it now. */
struct FVeyraShopOffer
{
	FVeyraContentId Item;
	int32 Tier = 0;

	/** Equipment, boots or a consumable: the quick-buy panels gather the last two (ADR-012 §11). */
	EVeyraItemCategory Category = EVeyraItemCategory::Equipment;

	/** Its whole recipe's cost, which orders the shop's lists. */
	double TotalCost = 0.0;

	/** What buying it costs now: its recipe, less the owned components it would consume (§11.1). */
	double Price = 0.0;

	/** Why it cannot be bought now; None when it can. */
	EVeyraShopRefusal Refusal = EVeyraShopRefusal::None;

	bool operator==(const FVeyraShopOffer&) const = default;
};

/** One inventory slot as the shop shows it. */
struct FVeyraShopSlot
{
	/** Invalid when the slot is empty. */
	FVeyraContentId Item;
	int32 Count = 0;

	/** What selling one returns (Economy & Progression Bible §12). */
	double SaleValue = 0.0;

	bool operator==(const FVeyraShopSlot&) const = default;
};

/** A purchase waiting for the fountain (§11), which may be cancelled for all its Gold. */
struct FVeyraShopPending
{
	FVeyraContentId Item;
	double Paid = 0.0;

	bool operator==(const FVeyraShopPending&) const = default;
};

/** A roster Flux Spell as a swap into one slot, or why it cannot be now. */
struct FVeyraShopSpellOffer
{
	FVeyraContentId Spell;
	EVeyraShopRefusal Refusal = EVeyraShopRefusal::None;
	bool operator==(const FVeyraShopSpellOffer&) const = default;
};

/** One Flux Spell slot as the shop shows it (ADR-015 §6). */
struct FVeyraShopSpellSlot
{
	/** Invalid when the slot is empty. */
	FVeyraContentId Spell;
	/** Its team's permanent Flux has not opened it yet; a swap still may, and it stays locked. */
	bool bLocked = false;
	/** Every roster spell, in roster order. */
	TArray<FVeyraShopSpellOffer> Offers;
	bool operator==(const FVeyraShopSpellSlot&) const = default;
};

/** A vision tool as a swap into the slot, or why it cannot be now (ADR-016 §6). */
struct FVeyraShopVisionToolOffer
{
	EVeyraVisionTool Tool = EVeyraVisionTool::PersistentWard;
	EVeyraShopRefusal Refusal = EVeyraShopRefusal::None;
	bool operator==(const FVeyraShopVisionToolOffer&) const = default;
};

/** What the shop shows its participant. */
struct FVeyraShopView
{
	double Gold = 0.0;

	/** Whether purchases arrive at once and selling and undo work: at the fountain, or dead (ADR-012 §7, §9). */
	bool bAtShop = false;

	/** How many of this visit's purchases undo can take back. */
	int32 UndoSteps = 0;

	/** Every item, by tier, then whole cost, then ID. */
	TArray<FVeyraShopOffer> Offers;

	/** The inventory's slots, in order. */
	TArray<FVeyraShopSlot> Slots;

	/** Purchases waiting for the fountain, in order. */
	TArray<FVeyraShopPending> Pending;
	/** The two Flux Spell slots, in slot order, and what a swap costs (Economy & Progression Bible §13.2). */
	TArray<FVeyraShopSpellSlot> SpellSlots;
	double SpellSwapCost = 0.0;

	/** The vision tool in the slot, and each tool as a swap, for the same cost each time (Vision Bible §3). */
	bool bHasVisionTool = false;
	EVeyraVisionTool VisionTool = EVeyraVisionTool::PersistentWard;
	TArray<FVeyraShopVisionToolOffer> VisionToolOffers;
	double VisionToolSwapCost = 0.0;

	/** Shown while the Vanguard is dead in a match that has buyback: its price now, and why not if not (§15). */
	bool bBuybackShown = false;
	FVeyraBuybackQuote Buyback;

	bool operator==(const FVeyraShopView&) const = default;
};

/**
 * The shop screen's model (ADR-012 §11): what it shows, from the participant's replicated Gold and
 * inventory and the catalog, priced by the inventory rules the server uses. It decides nothing; the
 * server checks every request again.
 */
namespace VeyraShopModel
{
	/** What the shop shows Participant, from what its owner sees. */
	VEYRAUI_API FVeyraShopView Describe(const AActor& Participant);

	/** An item's stats as the shop lists them, such as "+20 Physical Power, +150 Health". Empty for none. */
	VEYRAUI_API FText DescribeStats(const FVeyraItemStatsTuning& Stats);

	/** Why the shop refuses, in the player's words. */
	VEYRAUI_API FText DescribeRefusal(EVeyraShopRefusal Refusal);

	/** The items Item is a component of, by tier, then whole cost, then ID, as the shop lists them. */
	VEYRAUI_API TArray<FVeyraContentId> BuildsInto(const FVeyraItemsTuning& Tuning, const FVeyraContentId& Item);

	/** Why a buyback cannot be bought, as the shop says it. Empty for None. */
	VEYRAUI_API FText DescribeBuybackRefusal(EVeyraBuybackRefusal Refusal);
}
