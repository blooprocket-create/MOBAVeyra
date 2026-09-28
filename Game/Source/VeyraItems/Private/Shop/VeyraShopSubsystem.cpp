// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shop/VeyraShopSubsystem.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "Gold/VeyraGoldComponent.h"
#include "Inventory/VeyraEquipmentRules.h"
#include "Inventory/VeyraInventoryComponent.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Stats/VeyraEquipmentStats.h"
#include "Targeting/VeyraTargeting.h"
#include "TimerManager.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"
#include "Tuning/VeyraItemsTuningSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "VeyraItemsLog.h"

namespace
{
	/** Whether any slot that differs from its state in Before has given benefit: then the change cannot be undone (§12). */
	bool ChangedSlotBenefited(TConstArrayView<FVeyraInventorySlot> Before, TConstArrayView<FVeyraInventorySlot> After)
	{
		for (int32 Index = 0; Index < After.Num(); ++Index)
		{
			const FVeyraInventorySlot& Now = After[Index];
			const bool bChanged = !Before.IsValidIndex(Index) || Before[Index].Item != Now.Item || Before[Index].Count != Now.Count;
			if (bChanged && !Now.IsEmpty() && Now.bBenefited)
			{
				return true;
			}
		}
		return false;
	}
}

EVeyraShopRefusal UVeyraShopSubsystem::Buy(AActor& Participant, const FVeyraContentId& Item)
{
	UVeyraInventoryComponent* Inventory = Participant.FindComponentByClass<UVeyraInventoryComponent>();
	UVeyraGoldComponent* Gold = Participant.FindComponentByClass<UVeyraGoldComponent>();
	if (!Inventory || !Gold)
	{
		return EVeyraShopRefusal::NotNow;
	}
	const bool bAtShop = IsAtShop(Participant, *Inventory);
	if (bAtShop && !Inventory->Queue.IsEmpty())
	{
		// Arriving and dying deliver the queue; nothing should wait here, but never leave it waiting.
		Deliver(Participant);
	}
	const FVeyraItemsTuning& Tuning = UVeyraItemsTuningSubsystem::Get();
	const FVeyraPurchaseQuote Quote = VeyraInventory::Quote(Tuning, Inventory->Slots, Inventory->Queue, Item);
	if (Quote.Refusal != EVeyraShopRefusal::None)
	{
		return Quote.Refusal;
	}
	FVeyraPendingPurchase Entry;
	Entry.Item = Item;
	Entry.Paid = Quote.Price;
	Entry.Needs = Quote.Needs;

	if (bAtShop)
	{
		if (!Gold->Spend(Quote.Price))
		{
			return EVeyraShopRefusal::NotEnoughGold;
		}
		FVeyraUndoStep Step;
		Step.Paid = Quote.Price;
		Step.SlotsBefore = Inventory->Slots;
		TArray<FVeyraInventorySlot> Slots = Inventory->Slots;
		const EVeyraShopRefusal Refusal = VeyraInventory::Apply(Tuning, Slots, Entry);
		check(Refusal == EVeyraShopRefusal::None);
		Inventory->SetSlots(MoveTemp(Slots));
		Inventory->UndoSteps.Add(MoveTemp(Step));
		ApplyItems(Participant);
	}
	else
	{
		const TOptional<int32> Hold = Gold->Hold(Quote.Price);
		if (!Hold.IsSet())
		{
			return EVeyraShopRefusal::NotEnoughGold;
		}
		Entry.GoldHold = Hold.GetValue();
		TArray<FVeyraPendingPurchase> Queue = Inventory->Queue;
		Queue.Add(MoveTemp(Entry));
		Inventory->SetQueue(MoveTemp(Queue));
	}
	UE_LOG(LogVeyraItems, Log, TEXT("%s bought %s for %.0f Gold%s."), *GetNameSafe(&Participant), *Item.ToString(), Quote.Price,
		bAtShop ? TEXT("") : TEXT(", waiting for the fountain"));
	return EVeyraShopRefusal::None;
}

EVeyraShopRefusal UVeyraShopSubsystem::Cancel(AActor& Participant, int32 Index)
{
	UVeyraInventoryComponent* Inventory = Participant.FindComponentByClass<UVeyraInventoryComponent>();
	UVeyraGoldComponent* Gold = Participant.FindComponentByClass<UVeyraGoldComponent>();
	if (!Inventory || !Gold || !Inventory->Queue.IsValidIndex(Index))
	{
		return EVeyraShopRefusal::EmptySlot;
	}
	TArray<FVeyraPendingPurchase> Queue = Inventory->Queue;
	const FVeyraPendingPurchase Cancelled = Queue[Index];
	Queue.RemoveAt(Index);
	Inventory->SetQueue(MoveTemp(Queue));
	Gold->ReleaseHold(Cancelled.GoldHold);
	Revalidate(*Inventory, *Gold);
	return EVeyraShopRefusal::None;
}

EVeyraShopRefusal UVeyraShopSubsystem::Sell(AActor& Participant, int32 Slot)
{
	UVeyraInventoryComponent* Inventory = Participant.FindComponentByClass<UVeyraInventoryComponent>();
	UVeyraGoldComponent* Gold = Participant.FindComponentByClass<UVeyraGoldComponent>();
	if (!Inventory || !Gold)
	{
		return EVeyraShopRefusal::NotNow;
	}
	if (!IsAtShop(Participant, *Inventory))
	{
		return EVeyraShopRefusal::NotAtFountain;
	}
	if (!Inventory->Slots.IsValidIndex(Slot) || Inventory->Slots[Slot].IsEmpty())
	{
		return EVeyraShopRefusal::EmptySlot;
	}
	TArray<FVeyraInventorySlot> Slots = Inventory->Slots;
	const double Value = VeyraInventory::ResaleValue(UVeyraItemsTuningSubsystem::Get(), Slots[Slot]);
	const FVeyraContentId Sold = Slots[Slot].Item;
	if (--Slots[Slot].Count == 0)
	{
		Slots[Slot] = FVeyraInventorySlot();
	}
	Inventory->SetSlots(MoveTemp(Slots));
	// A sale changes what an undo would restore: the steps no longer describe the slots.
	Inventory->UndoSteps.Reset();
	Gold->Grant(Value, EVeyraGoldReason::Sale);
	Revalidate(*Inventory, *Gold);
	ApplyItems(Participant);
	UE_LOG(LogVeyraItems, Log, TEXT("%s sold %s for %.0f Gold."), *GetNameSafe(&Participant), *Sold.ToString(), Value);
	return EVeyraShopRefusal::None;
}

EVeyraShopRefusal UVeyraShopSubsystem::Undo(AActor& Participant)
{
	UVeyraInventoryComponent* Inventory = Participant.FindComponentByClass<UVeyraInventoryComponent>();
	UVeyraGoldComponent* Gold = Participant.FindComponentByClass<UVeyraGoldComponent>();
	if (!Inventory || !Gold)
	{
		return EVeyraShopRefusal::NotNow;
	}
	if (!IsAtShop(Participant, *Inventory))
	{
		return EVeyraShopRefusal::NotAtFountain;
	}
	if (Inventory->UndoSteps.IsEmpty())
	{
		return EVeyraShopRefusal::NothingToUndo;
	}
	const FVeyraUndoStep& Step = Inventory->UndoSteps.Last();
	if (ChangedSlotBenefited(Step.SlotsBefore, Inventory->Slots))
	{
		return EVeyraShopRefusal::AlreadyUsed;
	}
	const double Paid = Step.Paid;
	Inventory->SetSlots(Step.SlotsBefore);
	Inventory->UndoSteps.Pop();
	Gold->Grant(Paid, EVeyraGoldReason::Undo);
	ApplyItems(Participant);
	return EVeyraShopRefusal::None;
}

void UVeyraShopSubsystem::SetAtFountain(AActor& Participant, bool bAtFountain)
{
	UVeyraInventoryComponent* Inventory = Participant.FindComponentByClass<UVeyraInventoryComponent>();
	if (!Inventory || Inventory->bAtFountain == bAtFountain)
	{
		return;
	}
	Inventory->bAtFountain = bAtFountain;
	if (bAtFountain)
	{
		Deliver(Participant);
	}
	else
	{
		Inventory->UndoSteps.Reset();
	}
}

void UVeyraShopSubsystem::DeliverOnDeath(AActor& Participant)
{
	Deliver(Participant);
}

void UVeyraShopSubsystem::InitializeInventory(AActor& Participant)
{
	if (UVeyraInventoryComponent* Inventory = Participant.FindComponentByClass<UVeyraInventoryComponent>())
	{
		TArray<FVeyraInventorySlot> Slots;
		Slots.SetNum(UVeyraItemsTuningSubsystem::Get().Shop.InventorySlots);
		Inventory->SetSlots(MoveTemp(Slots));
	}
}

void UVeyraShopSubsystem::ApplyItems(AActor& Participant)
{
	const UVeyraInventoryComponent* Inventory = Participant.FindComponentByClass<UVeyraInventoryComponent>();
	UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Participant);
	if (!Inventory || !AbilitySystem)
	{
		return;
	}
	// Bonus Attack Speed is a fraction of the base, beside level growth, so the two add (ADR-012 §6).
	const UVeyraProgressionComponent* Progression = Participant.FindComponentByClass<UVeyraProgressionComponent>();
	const FVeyraItemsTuning& Tuning = UVeyraItemsTuningSubsystem::Get();
	const FVeyraEquipmentStats Stats = VeyraEquipment::StatsFor(Tuning, Inventory->Slots, Progression ? Progression->GetBaseAttackSpeed() : 0.0);
	VeyraCombat::SetEquipmentStats(*AbilitySystem, Stats);

	// Each item slot holds its item's Active, if it has one (ADR-012 §1).
	UVeyraAbilityLoadoutComponent* Loadout = Participant.FindComponentByClass<UVeyraAbilityLoadoutComponent>();
	if (!Loadout)
	{
		return;
	}
	for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(VeyraAbilitySlots::Items)); ++Index)
	{
		const EVeyraAbilitySlot Slot = VeyraAbilitySlots::Items[Index];
		const FVeyraInventorySlot* Held = Inventory->Slots.IsValidIndex(Index) && !Inventory->Slots[Index].IsEmpty() ? &Inventory->Slots[Index] : nullptr;
		const FVeyraItemDefinition* Item = Held ? Tuning.Items.Find(Held->Item) : nullptr;
		const FVeyraContentId Active = Item && !Item->Active.IsEmpty() ? Item->Active[0] : FVeyraContentId();
		const FVeyraLoadoutEntry* Current = Loadout->FindSlot(Slot);
		if (!Active.IsValid())
		{
			Loadout->Clear(*AbilitySystem, Slot);
		}
		else if (!Current || Current->Ability != Active)
		{
			Loadout->Grant(*AbilitySystem, Slot, Active);
		}
	}
}

EVeyraItemUse UVeyraShopSubsystem::GetUse(const AActor& Participant, int32 Index)
{
	const UVeyraInventoryComponent* Inventory = Participant.FindComponentByClass<UVeyraInventoryComponent>();
	const FVeyraInventorySlot* Held = Inventory && Inventory->Slots.IsValidIndex(Index) && !Inventory->Slots[Index].IsEmpty() ? &Inventory->Slots[Index] : nullptr;
	const FVeyraItemDefinition* Item = Held ? UVeyraItemsTuningSubsystem::FindItem(Held->Item) : nullptr;
	if (!Item)
	{
		return EVeyraItemUse::None;
	}
	if (Item->Category == EVeyraItemCategory::Consumable)
	{
		return EVeyraItemUse::Consumable;
	}
	return Item->Active.IsEmpty() ? EVeyraItemUse::None : EVeyraItemUse::Active;
}

EVeyraShopRefusal UVeyraShopSubsystem::UseConsumable(AActor& Participant, int32 Index)
{
	UVeyraInventoryComponent* Inventory = Participant.FindComponentByClass<UVeyraInventoryComponent>();
	if (!Inventory || GetUse(Participant, Index) != EVeyraItemUse::Consumable)
	{
		return EVeyraShopRefusal::EmptySlot;
	}
	if (!VeyraTargeting::IsAlive(&Participant))
	{
		return EVeyraShopRefusal::NotNow;
	}
	if (Restorations.ContainsByPredicate([&Participant](const FRestoration& Running) { return Running.Participant.Get() == &Participant; }))
	{
		return EVeyraShopRefusal::StillRestoring;
	}
	const FVeyraContentId Used = Inventory->Slots[Index].Item;
	const FVeyraConsumableTuning* Consumable = UVeyraItemsTuningSubsystem::Get().Consumables.Find(Used);
	if (!Consumable)
	{
		return EVeyraShopRefusal::UnknownItem;
	}
	TArray<FVeyraInventorySlot> Slots = Inventory->Slots;
	if (--Slots[Index].Count == 0)
	{
		Slots[Index] = FVeyraInventorySlot();
	}
	Inventory->SetSlots(MoveTemp(Slots));
	// A used consumable has given benefit; no undo may bring it back (§12).
	Inventory->UndoSteps.Reset();

	// Restores in equal parts on Combat's regeneration tick, on world time, so a pause holds it.
	const double TickSeconds = UVeyraCombatTuningSubsystem::Get().Regeneration.TickSeconds;
	FRestoration& Restoration = Restorations.AddDefaulted_GetRef();
	Restoration.Participant = &Participant;
	Restoration.TicksLeft = FMath::Max(1, FMath::RoundToInt32(Consumable->DurationSeconds / TickSeconds));
	Restoration.PerTick = Consumable->HealthRestored / Restoration.TicksLeft;
	if (!GetWorld()->GetTimerManager().IsTimerActive(RestorationTimer))
	{
		GetWorld()->GetTimerManager().SetTimer(RestorationTimer, FTimerDelegate::CreateUObject(this, &UVeyraShopSubsystem::OnRestorationTimer),
			static_cast<float>(TickSeconds), /*bLoop*/ true);
	}
	ApplyItems(Participant);
	UE_LOG(LogVeyraItems, Log, TEXT("%s used %s."), *GetNameSafe(&Participant), *Used.ToString());
	return EVeyraShopRefusal::None;
}

void UVeyraShopSubsystem::NoteActiveUsed(AActor& Participant, int32 Index)
{
	UVeyraInventoryComponent* Inventory = Participant.FindComponentByClass<UVeyraInventoryComponent>();
	if (!Inventory || !Inventory->Slots.IsValidIndex(Index) || Inventory->Slots[Index].IsEmpty())
	{
		return;
	}
	TArray<FVeyraInventorySlot> Slots = Inventory->Slots;
	Slots[Index].bBenefited = true;
	Inventory->SetSlots(MoveTemp(Slots));
	Inventory->UndoSteps.Reset();
}

void UVeyraShopSubsystem::OnRestorationTimer()
{
	for (int32 Index = Restorations.Num() - 1; Index >= 0; --Index)
	{
		FRestoration& Restoration = Restorations[Index];
		AActor* Participant = Restoration.Participant.Get();
		UAbilitySystemComponent* AbilitySystem = Participant ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Participant) : nullptr;
		// Death ends it, as it ends every effect with a duration (Combat Bible §44).
		if (!AbilitySystem || !VeyraTargeting::IsAlive(Participant))
		{
			Restorations.RemoveAt(Index);
			continue;
		}
		VeyraCombat::RestoreHealth(*AbilitySystem, Restoration.PerTick);
		if (--Restoration.TicksLeft <= 0)
		{
			Restorations.RemoveAt(Index);
		}
	}
	if (Restorations.IsEmpty())
	{
		GetWorld()->GetTimerManager().ClearTimer(RestorationTimer);
	}
}

void UVeyraShopSubsystem::Deinitialize()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RestorationTimer);
	}
	Restorations.Reset();
	Super::Deinitialize();
}

bool UVeyraShopSubsystem::IsAtShop(const AActor& Participant, const UVeyraInventoryComponent& Inventory)
{
	return Inventory.bAtFountain || !VeyraTargeting::IsAlive(&Participant);
}

void UVeyraShopSubsystem::Deliver(AActor& Participant)
{
	UVeyraInventoryComponent* Inventory = Participant.FindComponentByClass<UVeyraInventoryComponent>();
	UVeyraGoldComponent* Gold = Participant.FindComponentByClass<UVeyraGoldComponent>();
	if (!Inventory || !Gold || Inventory->Queue.IsEmpty())
	{
		return;
	}
	Revalidate(*Inventory, *Gold);
	const FVeyraItemsTuning& Tuning = UVeyraItemsTuningSubsystem::Get();
	TArray<FVeyraInventorySlot> Slots = Inventory->Slots;
	for (const FVeyraPendingPurchase& Entry : Inventory->Queue)
	{
		// Each delivered purchase can be undone at the fountain like one bought there (§12).
		FVeyraUndoStep Step;
		Step.Paid = Entry.Paid;
		Step.SlotsBefore = Slots;
		const EVeyraShopRefusal Refusal = VeyraInventory::Apply(Tuning, Slots, Entry);
		check(Refusal == EVeyraShopRefusal::None);
		Gold->SettleHold(Entry.GoldHold);
		Inventory->UndoSteps.Add(MoveTemp(Step));
	}
	UE_LOG(LogVeyraItems, Log, TEXT("%s received %d purchase(s) at the fountain."), *GetNameSafe(&Participant), Inventory->Queue.Num());
	Inventory->SetSlots(MoveTemp(Slots));
	Inventory->SetQueue(TArray<FVeyraPendingPurchase>());
	ApplyItems(Participant);
}

void UVeyraShopSubsystem::Revalidate(UVeyraInventoryComponent& Inventory, UVeyraGoldComponent& Gold)
{
	const FVeyraItemsTuning& Tuning = UVeyraItemsTuningSubsystem::Get();
	TArray<FVeyraPendingPurchase> Queue = Inventory.Queue;
	bool bChanged = false;
	for (;;)
	{
		TArray<FVeyraInventorySlot> After;
		const TArray<int32> Invalid = VeyraInventory::Simulate(Tuning, Inventory.Slots, Queue, After);
		if (Invalid.IsEmpty())
		{
			break;
		}
		// The first invalid entry; replaying again shows what depended on it.
		Gold.ReleaseHold(Queue[Invalid[0]].GoldHold);
		Queue.RemoveAt(Invalid[0]);
		bChanged = true;
	}
	if (bChanged)
	{
		Inventory.SetQueue(MoveTemp(Queue));
	}
}
