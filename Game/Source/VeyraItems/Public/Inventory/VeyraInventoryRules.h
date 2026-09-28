// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "UObject/ObjectMacros.h"

#include "VeyraInventoryRules.generated.h"

struct FVeyraItemsTuning;

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

	bool IsEmpty() const { return !Item.IsValid() || Count <= 0; }
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
	/** The match is paused or over (Match Flow §10.2). */
	NotNow,
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
	 * part of it and consuming the owned ones the queue leaves, as deep as the recipe goes.
	 */
	VEYRAITEMS_API FVeyraPurchaseQuote Quote(const FVeyraItemsTuning& Tuning, TConstArrayView<FVeyraInventorySlot> Slots,
		TConstArrayView<FVeyraPendingPurchase> Queue, const FVeyraContentId& Item);

	/** What selling one of Slot's items returns (§12): its consumable's own fraction, else the shop's, of what it cost. */
	VEYRAITEMS_API double ResaleValue(const FVeyraItemsTuning& Tuning, const FVeyraInventorySlot& Slot);
}
