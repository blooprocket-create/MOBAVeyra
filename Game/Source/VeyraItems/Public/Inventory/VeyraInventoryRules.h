// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "UObject/ObjectMacros.h"

#include "VeyraInventoryRules.generated.h"

struct FVeyraItemsTuning;

/** What an item's Attunements keep on its slot (ADR-025 §7). */
enum class EVeyraItemStore : uint8
{
	/** Residual Current's and High Tide's Current. */
	Current,
	/** Safe Harbor's Reserve. */
	Reserve,
};

/** One inventory slot: an item, or a stack of a consumable, and the Gold its present form cost. Empty when Item is invalid. */
USTRUCT()
struct FVeyraInventorySlot
{
	GENERATED_BODY()

	UPROPERTY()
	FVeyraContentId Item;

	UPROPERTY()
	int32 Count = 0;

	/** Gold spent on each one's present form, components and recipes included (Economy §12). */
	UPROPERTY()
	double PaidEach = 0.0;

	/** Whether it has been used, consumed or activated, which ends undo (§12). */
	UPROPERTY()
	bool bBenefited = false;

	/** A refillable consumable's charges left (Item Bible §12; ADR-023 §6); 0 for everything else. */
	UPROPERTY()
	int32 Charges = 0;

	/**
	 * Max Health an Attunement grew into this item, as Tempered by Conflict does (ADR-023 §3): part of
	 * the item's stats, so it leaves with the item.
	 */
	UPROPERTY()
	double GrownHealth = 0.0;

	/** Progress toward its quest (Item Bible §2.5; ADR-025 §3): lost if the item is sold, reset when it evolves. */
	UPROPERTY()
	int32 QuestProgress = 0;

	/**
	 * Current its Residual Current stores (ADR-025 §7): part of the item, so it survives death and
	 * leaves with the item.
	 */
	UPROPERTY()
	double Current = 0.0;

	/** Reserve its Safe Harbor banks (ADR-025 §7): part of the item, like Current. */
	UPROPERTY()
	double Reserve = 0.0;

	double Stored(EVeyraItemStore Store) const { return Store == EVeyraItemStore::Current ? Current : Reserve; }
	double& Stored(EVeyraItemStore Store) { return Store == EVeyraItemStore::Current ? Current : Reserve; }

	bool IsEmpty() const { return !Item.IsValid() || Count <= 0; }

	bool operator==(const FVeyraInventorySlot&) const = default;
};

/**
 * A purchase paid for and not yet delivered (Economy & Progression Bible §11.1): the item it delivers,
 * the Gold it paid, and the owned items it consumes at delivery. The components it had to buy were
 * bought as part of it, so no intermediate item ever exists.
 */
USTRUCT()
struct FVeyraPendingPurchase
{
	GENERATED_BODY()

	UPROPERTY()
	FVeyraContentId Item;

	UPROPERTY()
	double Paid = 0.0;

	UPROPERTY()
	TArray<FVeyraContentId> Needs;

	/** The Gold hold that paid for it, in Economy. */
	UPROPERTY()
	int32 GoldHold = 0;

	/** Whether this purchase chose the participant's Mythical: cancelling it releases the choice (ADR-025 §2). */
	UPROPERTY()
	bool bSetsMythical = false;
};

/** Why the shop refuses a request. */
UENUM()
enum class EVeyraShopRefusal : uint8
{
	None,
	/** The catalog has no such item. */
	UnknownItem,
	/** No slot would hold it once the queue delivers (§11.1). */
	InventoryFull,
	/** A Masterwork is held once (ADR-012 §9). */
	Unique,
	/** One pair of Boots (ADR-012 §9). */
	BootsLimit,
	NotEnoughGold,
	/** Selling and undo happen only at the fountain (§12). */
	NotAtFountain,
	NothingToUndo,
	/** The item gave benefit: it can be sold, not undone (§12). */
	AlreadyUsed,
	EmptySlot,
	/** A component the pending purchase needs is gone (§11.3). */
	MissingComponent,
	/** The match is paused or over (Match Flow §10.2), or the Vanguard is dead. */
	NotNow,
	/** A consumable is still restoring; another waits until it ends. */
	StillRestoring,
	/** The Flux Spell roster has no such spell (ADR-015 §6). */
	UnknownSpell,
	/** A Vanguard has two Flux Spell slots. */
	NoSuchSpellSlot,
	/** One of the Vanguard's slots holds that spell already. */
	AlreadyEquipped,
	/** A refillable consumable with no charge left: it refills at the fountain and from a Flux Well (ADR-023 §6). */
	NoCharges,
	/** Another Mythical is the participant's for this match (Item Bible §11; ADR-025 §2). */
	MythicalTaken,
	/** The shop never sells it, or a part the recipe still needs: only a quest makes it (ADR-025 §3). */
	NotForSale,
	/** An item of its quest line is held or waiting already: one at a time (Item Bible §2.5). */
	QuestLineHeld,
};

VEYRAITEMS_API const TCHAR* LexToString(EVeyraShopRefusal Refusal);

/** What buying an item costs from a given inventory and queue, or why it cannot be bought. */
struct FVeyraPurchaseQuote
{
	double Price = 0.0;
	TArray<FVeyraContentId> Needs;
	EVeyraShopRefusal Refusal = EVeyraShopRefusal::None;
};

/**
 * The inventory and purchase-queue rules of the Economy & Progression Bible §10–§12 as pure functions
 * of the catalog (ADR-012 §5). Slots are a fixed-size array whose empty entries are free. Validation is
 * replaying the queue on a copy of the slots: an entry whose needs are gone, or that no longer fits, is
 * invalid, and so is anything that needed what it would have delivered.
 */
namespace VeyraInventory
{
	/**
	 * Delivers Entry into Slots: consumes its needs, then places its item. Returns why it cannot, and
	 * then Slots are unchanged.
	 */
	VEYRAITEMS_API EVeyraShopRefusal Apply(const FVeyraItemsTuning& Tuning, TArray<FVeyraInventorySlot>& Slots, const FVeyraPendingPurchase& Entry);

	/** Replays Queue on a copy of Slots into OutSlots; returns the indices of the entries that cannot deliver. */
	VEYRAITEMS_API TArray<int32> Simulate(const FVeyraItemsTuning& Tuning, TConstArrayView<FVeyraInventorySlot> Slots,
		TConstArrayView<FVeyraPendingPurchase> Queue, TArray<FVeyraInventorySlot>& OutSlots);

	/**
	 * What buying Item costs once Queue delivers (§11.1): its recipe, buying each missing component as
	 * part of it and consuming the owned ones the queue leaves, as deep as the recipe goes. Mythical is
	 * the participant's Mythical this match, invalid until one is bought: no other can be (ADR-025 §2).
	 */
	VEYRAITEMS_API FVeyraPurchaseQuote Quote(const FVeyraItemsTuning& Tuning, TConstArrayView<FVeyraInventorySlot> Slots,
		TConstArrayView<FVeyraPendingPurchase> Queue, const FVeyraContentId& Mythical, const FVeyraContentId& Item);

	/** What selling one of Slot's items returns (§12): its consumable's own fraction, else the shop's, of what it cost. */
	VEYRAITEMS_API double ResaleValue(const FVeyraItemsTuning& Tuning, const FVeyraInventorySlot& Slot);
}
