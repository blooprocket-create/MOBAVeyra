// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Inventory/VeyraInventoryRules.h"

#include "Tuning/VeyraItemsTuning.h"

const TCHAR* LexToString(EVeyraShopRefusal Refusal)
{
	switch (Refusal)
	{
	case EVeyraShopRefusal::None:
		return TEXT("none");
	case EVeyraShopRefusal::UnknownItem:
		return TEXT("no such item");
	case EVeyraShopRefusal::InventoryFull:
		return TEXT("no room once the queue delivers");
	case EVeyraShopRefusal::Unique:
		return TEXT("already held, and held once");
	case EVeyraShopRefusal::BootsLimit:
		return TEXT("already wearing Boots");
	case EVeyraShopRefusal::NotEnoughGold:
		return TEXT("not enough Gold");
	case EVeyraShopRefusal::NotAtFountain:
		return TEXT("only at the fountain");
	case EVeyraShopRefusal::NothingToUndo:
		return TEXT("nothing to undo");
	case EVeyraShopRefusal::AlreadyUsed:
		return TEXT("already used");
	case EVeyraShopRefusal::EmptySlot:
		return TEXT("an empty slot");
	case EVeyraShopRefusal::MissingComponent:
		return TEXT("a component is gone");
	case EVeyraShopRefusal::NotNow:
		return TEXT("not now");
	case EVeyraShopRefusal::StillRestoring:
		return TEXT("one is still restoring");
	case EVeyraShopRefusal::UnknownSpell:
		return TEXT("no such Flux Spell");
	case EVeyraShopRefusal::NoSuchSpellSlot:
		return TEXT("no such spell slot");
	case EVeyraShopRefusal::AlreadyEquipped:
		return TEXT("already equipped");
	case EVeyraShopRefusal::NoCharges:
		return TEXT("no charges left");
	case EVeyraShopRefusal::MythicalTaken:
		return TEXT("another Mythical is chosen");
	case EVeyraShopRefusal::NotForSale:
		return TEXT("only a quest makes it");
	case EVeyraShopRefusal::QuestLineHeld:
		return TEXT("an item of its quest line is held");
	}
	return TEXT("unknown");
}

namespace VeyraInventory
{
namespace
{
	/** Removes one Item from Slots, returning its slot as it was, or nothing if Slots hold none. */
	TOptional<FVeyraInventorySlot> Take(TArray<FVeyraInventorySlot>& Slots, const FVeyraContentId& Item)
	{
		for (FVeyraInventorySlot& Slot : Slots)
		{
			if (!Slot.IsEmpty() && Slot.Item == Item)
			{
				const FVeyraInventorySlot Taken = Slot;
				if (--Slot.Count == 0)
				{
					Slot = FVeyraInventorySlot();
				}
				return Taken;
			}
		}
		return {};
	}

	/**
	 * Gold to obtain Item from Slots, consuming owned items where it can, recursively as League does,
	 * and recording each one consumed in Needs. Item itself is never taken: it is being bought. A
	 * missing part the shop never sells sets bNeedsUnsold, and is not bought (ADR-025 §3).
	 */
	double Resolve(const FVeyraItemsTuning& Tuning, TArray<FVeyraInventorySlot>& Slots, const FVeyraItemDefinition& Definition, TArray<FVeyraContentId>& Needs,
		bool& bNeedsUnsold)
	{
		double Price = Definition.Cost;
		for (const FVeyraContentId& Component : Definition.Components)
		{
			if (Take(Slots, Component).IsSet())
			{
				Needs.Add(Component);
			}
			else if (VeyraItems::IsEvolutionOnly(Tuning, Component))
			{
				bNeedsUnsold = true;
			}
			else if (const FVeyraItemDefinition* ComponentDefinition = Tuning.Items.Find(Component))
			{
				Price += Resolve(Tuning, Slots, *ComponentDefinition, Needs, bNeedsUnsold);
			}
		}
		return Price;
	}

	/** Why Slots cannot receive one more Item now. */
	EVeyraShopRefusal RefusalToHold(const FVeyraItemsTuning& Tuning, TConstArrayView<FVeyraInventorySlot> Slots, const FVeyraContentId& Item)
	{
		const FVeyraItemDefinition* Definition = Tuning.Items.Find(Item);
		if (!Definition)
		{
			return EVeyraShopRefusal::UnknownItem;
		}
		int32 Boots = 0;
		bool bHeld = false;
		bool bRoomInStack = false;
		bool bFreeSlot = false;
		bool bLineHeld = false;
		const FVeyraContentId Line = VeyraItems::QuestLine(Tuning, Item);
		for (const FVeyraInventorySlot& Slot : Slots)
		{
			if (Slot.IsEmpty())
			{
				bFreeSlot = true;
				continue;
			}
			const FVeyraItemDefinition* Held = Tuning.Items.Find(Slot.Item);
			Boots += Held && Held->Category == EVeyraItemCategory::Boots ? Slot.Count : 0;
			bHeld |= Slot.Item == Item;
			bRoomInStack |= Slot.Item == Item && Slot.Count < Definition->StackLimit;
			bLineHeld |= Line.IsValid() && VeyraItems::QuestLine(Tuning, Slot.Item) == Line;
		}
		// One item of a quest line at a time, so its progress is never duplicated (Item Bible §2.5).
		if (bLineHeld)
		{
			return EVeyraShopRefusal::QuestLineHeld;
		}
		// A Masterwork is held once (ADR-012 §9), and so is a refillable consumable (Item Bible §12).
		const FVeyraConsumableTuning* Consumable = Tuning.Consumables.Find(Item);
		if (bHeld && (Definition->Tier >= Tuning.Shop.UniqueFromTier || (Consumable && Consumable->Charges > 0)))
		{
			return EVeyraShopRefusal::Unique;
		}
		if (Definition->Category == EVeyraItemCategory::Boots && Boots >= Tuning.Shop.MaxBoots)
		{
			return EVeyraShopRefusal::BootsLimit;
		}
		return bRoomInStack || bFreeSlot ? EVeyraShopRefusal::None : EVeyraShopRefusal::InventoryFull;
	}

	/** Puts one Item in Slots, on its stack or in a free slot; returns the slot it went to. */
	FVeyraInventorySlot* Place(const FVeyraItemsTuning& Tuning, TArray<FVeyraInventorySlot>& Slots, const FVeyraContentId& Item, double PaidEach)
	{
		const int32 StackLimit = Tuning.Items.FindChecked(Item).StackLimit;
		for (FVeyraInventorySlot& Slot : Slots)
		{
			if (!Slot.IsEmpty() && Slot.Item == Item && Slot.Count < StackLimit)
			{
				++Slot.Count;
				return &Slot;
			}
		}
		for (FVeyraInventorySlot& Slot : Slots)
		{
			if (Slot.IsEmpty())
			{
				Slot = FVeyraInventorySlot();
				Slot.Item = Item;
				Slot.Count = 1;
				Slot.PaidEach = PaidEach;
				// A refillable consumable arrives full.
				const FVeyraConsumableTuning* Consumable = Tuning.Consumables.Find(Item);
				Slot.Charges = Consumable ? Consumable->Charges : 0;
				return &Slot;
			}
		}
		return nullptr;
	}
}

EVeyraShopRefusal Apply(const FVeyraItemsTuning& Tuning, TArray<FVeyraInventorySlot>& Slots, const FVeyraPendingPurchase& Entry)
{
	TArray<FVeyraInventorySlot> Working = Slots;
	double ConsumedPaid = 0.0;
	FVeyraInventorySlot Carried;
	for (const FVeyraContentId& Need : Entry.Needs)
	{
		const TOptional<FVeyraInventorySlot> Taken = Take(Working, Need);
		if (!Taken.IsSet())
		{
			return EVeyraShopRefusal::MissingComponent;
		}
		ConsumedPaid += Taken->PaidEach;
		Carried.Current += Taken->Current;
		Carried.Reserve += Taken->Reserve;
	}
	const EVeyraShopRefusal Refusal = RefusalToHold(Tuning, Working, Entry.Item);
	if (Refusal != EVeyraShopRefusal::None)
	{
		return Refusal;
	}
	// A Mythical carries its parts' mechanics on (Item Bible §11): what they stored, it keeps (ADR-025 §7).
	if (FVeyraInventorySlot* Placed = Place(Tuning, Working, Entry.Item, ConsumedPaid + Entry.Paid))
	{
		for (const EVeyraItemStore Store : { EVeyraItemStore::Current, EVeyraItemStore::Reserve })
		{
			if (Stores(Tuning, Entry.Item, Store))
			{
				Placed->Stored(Store) += Carried.Stored(Store);
			}
		}
	}
	Slots = MoveTemp(Working);
	return EVeyraShopRefusal::None;
}

TArray<int32> Simulate(const FVeyraItemsTuning& Tuning, TConstArrayView<FVeyraInventorySlot> Slots, TConstArrayView<FVeyraPendingPurchase> Queue,
	TArray<FVeyraInventorySlot>& OutSlots)
{
	OutSlots = TArray<FVeyraInventorySlot>(Slots);
	TArray<int32> Invalid;
	for (int32 Index = 0; Index < Queue.Num(); ++Index)
	{
		if (Apply(Tuning, OutSlots, Queue[Index]) != EVeyraShopRefusal::None)
		{
			Invalid.Add(Index);
		}
	}
	return Invalid;
}

FVeyraPurchaseQuote Quote(const FVeyraItemsTuning& Tuning, TConstArrayView<FVeyraInventorySlot> Slots, TConstArrayView<FVeyraPendingPurchase> Queue,
	const FVeyraContentId& Mythical, const FVeyraContentId& Item)
{
	FVeyraPurchaseQuote Result;
	const FVeyraItemDefinition* Definition = Tuning.Items.Find(Item);
	if (!Definition)
	{
		Result.Refusal = EVeyraShopRefusal::UnknownItem;
		return Result;
	}
	TArray<FVeyraInventorySlot> After;
	Simulate(Tuning, Slots, Queue, After);
	bool bNeedsUnsold = false;
	Result.Price = Resolve(Tuning, After, *Definition, Result.Needs, bNeedsUnsold);
	// One Mythical per participant per match: the first bought stays theirs, even once sold (ADR-025 §2).
	const bool bMythicalTaken = VeyraItems::IsMythical(*Definition) && Mythical.IsValid() && Mythical != Item;
	// An evolved Quest Item comes only from its quest, and a recipe that needs one waits for it (ADR-025 §3).
	const bool bNotForSale = bNeedsUnsold || VeyraItems::IsEvolutionOnly(Tuning, Item);
	Result.Refusal = bMythicalTaken ? EVeyraShopRefusal::MythicalTaken
		: bNotForSale              ? EVeyraShopRefusal::NotForSale
								   : RefusalToHold(Tuning, After, Item);
	return Result;
}

bool Stores(const FVeyraItemsTuning& Tuning, const FVeyraContentId& Item, EVeyraItemStore Store)
{
	const FVeyraItemDefinition* Definition = Tuning.Items.Find(Item);
	return Definition && Definition->Attunement.ContainsByPredicate([&Tuning, Store](const FVeyraContentId& Attunement) {
		return Store == EVeyraItemStore::Current ? Tuning.ResidualCurrent.Contains(Attunement) || Tuning.HighTide.Contains(Attunement)
												 : Tuning.SafeHarbor.Contains(Attunement);
	});
}

double ResaleValue(const FVeyraItemsTuning& Tuning, const FVeyraInventorySlot& Slot)
{
	if (Slot.IsEmpty())
	{
		return 0.0;
	}
	const FVeyraConsumableTuning* Consumable = Tuning.Consumables.Find(Slot.Item);
	return Slot.PaidEach * (Consumable ? Consumable->ResaleFraction : Tuning.Shop.ResaleFraction);
}
}
