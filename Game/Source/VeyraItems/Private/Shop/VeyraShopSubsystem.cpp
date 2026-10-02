// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shop/VeyraShopSubsystem.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "Engine/World.h"
#include "Gold/VeyraGoldComponent.h"
#include "Inventory/VeyraEquipmentRules.h"
#include "Inventory/VeyraInventoryComponent.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Quests/VeyraQuestRules.h"
#include "Rewards/VeyraEconomyTuningSubsystem.h"
#include "Stats/VeyraEquipmentStats.h"
#include "Targeting/VeyraTargeting.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"
#include "Tuning/VeyraItemsTuningSubsystem.h"
#include "Units/VeyraUnit.h"
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
	const FVeyraPurchaseQuote Quote = VeyraInventory::Quote(Tuning, Inventory->Slots, Inventory->Queue, Inventory->Mythical, Item);
	if (Quote.Refusal != EVeyraShopRefusal::None)
	{
		return Quote.Refusal;
	}
	FVeyraPendingPurchase Entry;
	Entry.Item = Item;
	Entry.Paid = Quote.Price;
	Entry.Needs = Quote.Needs;
	// The first Mythical bought or queued is the participant's for the match (ADR-025 §2).
	const bool bSetsMythical = !Inventory->Mythical.IsValid() && VeyraItems::IsMythical(Tuning.Items.FindChecked(Item));
	Entry.bSetsMythical = bSetsMythical;

	if (bAtShop)
	{
		if (!Gold->Spend(Quote.Price))
		{
			return EVeyraShopRefusal::NotEnoughGold;
		}
		FVeyraUndoStep Step;
		Step.Paid = Quote.Price;
		Step.SlotsBefore = Inventory->Slots;
		Step.MythicalBefore = Inventory->Mythical;
		TArray<FVeyraInventorySlot> Slots = Inventory->Slots;
		const EVeyraShopRefusal Refusal = VeyraInventory::Apply(Tuning, Slots, Entry);
		check(Refusal == EVeyraShopRefusal::None);
		Inventory->SetSlots(MoveTemp(Slots));
		Inventory->AddUndoStep(MoveTemp(Step));
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
	if (bSetsMythical)
	{
		Inventory->SetMythical(Item);
	}
	UE_LOG(LogVeyraItems, Log, TEXT("%s bought %s for %.0f Gold%s."), *GetNameSafe(&Participant), *Item.ToString(), Quote.Price,
		bAtShop ? TEXT("") : TEXT(", waiting for the fountain"));
	return EVeyraShopRefusal::None;
}

EVeyraShopRefusal UVeyraShopSubsystem::GrantItem(AActor& Participant, const FVeyraContentId& Item)
{
	UVeyraInventoryComponent* Inventory = Participant.FindComponentByClass<UVeyraInventoryComponent>();
	UVeyraGoldComponent* Gold = Participant.FindComponentByClass<UVeyraGoldComponent>();
	if (!Inventory || !Gold)
	{
		return EVeyraShopRefusal::NotNow;
	}
	// A purchase's rules without its price: the same slots, limits and recipe, its owned components consumed.
	// Only an evolved Quest Item, which no purchase makes, may be given outright (ADR-025 §3).
	const FVeyraItemsTuning& Tuning = UVeyraItemsTuningSubsystem::Get();
	const FVeyraPurchaseQuote Quote = VeyraInventory::Quote(Tuning, Inventory->Slots, Inventory->Queue, Inventory->Mythical, Item);
	const bool bEvolvedGrant = Quote.Refusal == EVeyraShopRefusal::NotForSale && VeyraItems::IsEvolutionOnly(Tuning, Item);
	if (Quote.Refusal != EVeyraShopRefusal::None && !bEvolvedGrant)
	{
		return Quote.Refusal;
	}
	FVeyraPendingPurchase Entry;
	Entry.Item = Item;
	Entry.Needs = Quote.Needs;
	TArray<FVeyraInventorySlot> Slots = Inventory->Slots;
	if (const EVeyraShopRefusal Refusal = VeyraInventory::Apply(Tuning, Slots, Entry); Refusal != EVeyraShopRefusal::None)
	{
		return Refusal;
	}
	Inventory->SetSlots(MoveTemp(Slots));
	if (!Inventory->Mythical.IsValid() && VeyraItems::IsMythical(Tuning.Items.FindChecked(Item)))
	{
		Inventory->SetMythical(Item);
	}
	// The grant changes what an undo would restore: the steps no longer describe the slots.
	Inventory->ResetUndoSteps();
	Revalidate(*Inventory, *Gold);
	ApplyItems(Participant);
	UE_LOG(LogVeyraItems, Log, TEXT("%s was given %s."), *GetNameSafe(&Participant), *Item.ToString());
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
	if (Cancelled.bSetsMythical)
	{
		Inventory->SetMythical(FVeyraContentId());
	}
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
	Inventory->ResetUndoSteps();
	Gold->Grant(Value, EVeyraGoldReason::Sale);
	Revalidate(*Inventory, *Gold);
	ApplyItems(Participant);
	UE_LOG(LogVeyraItems, Log, TEXT("%s sold %s for %.0f Gold."), *GetNameSafe(&Participant), *Sold.ToString(), Value);
	return EVeyraShopRefusal::None;
}

EVeyraShopRefusal UVeyraShopSubsystem::ChargeAtFountain(AActor& Participant, double Cost, const TCHAR* ForWhat)
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
	if (!Gold->Spend(Cost))
	{
		return EVeyraShopRefusal::NotEnoughGold;
	}
	UE_LOG(LogVeyraItems, Log, TEXT("%s paid %.0f Gold for %s."), *GetNameSafe(&Participant), Cost, ForWhat);
	return EVeyraShopRefusal::None;
}

EVeyraShopRefusal UVeyraShopSubsystem::SwapFluxSpell(AActor& Participant, int32 Slot, const FVeyraContentId& Spell)
{
	UVeyraInventoryComponent* Inventory = Participant.FindComponentByClass<UVeyraInventoryComponent>();
	UVeyraGoldComponent* Gold = Participant.FindComponentByClass<UVeyraGoldComponent>();
	UVeyraAbilityLoadoutComponent* Loadout = Participant.FindComponentByClass<UVeyraAbilityLoadoutComponent>();
	UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Participant);
	if (!Inventory || !Gold || !Loadout || !AbilitySystem)
	{
		return EVeyraShopRefusal::NotNow;
	}
	// Only at its own fountain: a remote purchase never changes a spell (§14).
	if (!IsAtShop(Participant, *Inventory))
	{
		return EVeyraShopRefusal::NotAtFountain;
	}
	if (Slot < 0 || Slot >= static_cast<int32>(UE_ARRAY_COUNT(VeyraAbilitySlots::Spells)))
	{
		return EVeyraShopRefusal::NoSuchSpellSlot;
	}
	const FVeyraAbilitiesTuning& Abilities = UVeyraAbilitiesTuningSubsystem::Get();
	if (!Abilities.FluxSpells.Roster.Contains(Spell))
	{
		return EVeyraShopRefusal::UnknownSpell;
	}
	for (const EVeyraAbilitySlot SpellSlot : VeyraAbilitySlots::Spells)
	{
		const FVeyraLoadoutEntry* Equipped = Loadout->FindSlot(SpellSlot);
		if (Equipped && Equipped->Ability == Spell)
		{
			return EVeyraShopRefusal::AlreadyEquipped;
		}
	}
	const double Cost = UVeyraEconomyTuningSubsystem::Get().FluxSpells.SwapCost;
	if (!Gold->Spend(Cost))
	{
		return EVeyraShopRefusal::NotEnoughGold;
	}
	const EVeyraAbilitySlot Target = VeyraAbilitySlots::Spells[Slot];
	const FVeyraLoadoutEntry* Replaced = Loadout->FindSlot(Target);
	const FVeyraContentId Previous = Replaced ? Replaced->Ability : FVeyraContentId();
	UVeyraCooldownComponent* Cooldowns = Participant.FindComponentByClass<UVeyraCooldownComponent>();
	const bool bWasCooling = Previous.IsValid() && Cooldowns && Cooldowns->GetRemainingSecondsNow(Previous) > 0.0;
	Loadout->Grant(*AbilitySystem, Target, Spell);
	// A swap never resets a cooldown and never inherits one: the new spell takes the slot's state. The
	// ledger keys cooldowns by spell, so a spell swapped back in forgets what it had before (ADR-015 §6).
	if (bWasCooling)
	{
		Cooldowns->StartCooldown(Spell, VeyraAbilityRules::CooldownSeconds(Abilities, Spell, 1), EVeyraCooldownHaste::Fixed);
	}
	else if (Cooldowns)
	{
		Cooldowns->ClearCooldown(Spell);
	}
	UE_LOG(LogVeyraItems, Log, TEXT("%s swapped Flux Spell slot %d from %s to %s for %.0f Gold."), *GetNameSafe(&Participant), Slot + 1,
		Previous.IsValid() ? *Previous.ToString() : TEXT("(empty)"), *Spell.ToString(), Cost);
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
	Inventory->SetMythical(Step.MythicalBefore);
	Inventory->PopUndoStep();
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
	Inventory->SetAtFountainState(bAtFountain);
	if (bAtFountain)
	{
		Deliver(Participant);
		RefillCharges(Participant);
	}
	else
	{
		Inventory->ResetUndoSteps();
	}
}

void UVeyraShopSubsystem::DeliverOnDeath(AActor& Participant)
{
	// Death ends what the Attunements built up, as it ends every temporary effect (Combat Bible §44).
	if (UVeyraInventoryComponent* Inventory = Participant.FindComponentByClass<UVeyraInventoryComponent>(); Inventory && !Inventory->Stacks.IsEmpty())
	{
		Inventory->Stacks.Reset();
		ApplyItems(Participant);
	}
	Deliver(Participant);
}

void UVeyraShopSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UVeyraCombatEventSubsystem* Events = Collection.InitializeDependency<UVeyraCombatEventSubsystem>())
	{
		HostileDamageHandle = Events->OnHostileDamage.AddUObject(this, &UVeyraShopSubsystem::OnHostileDamage);
		DeathHandle = Events->OnDeath.AddUObject(this, &UVeyraShopSubsystem::OnDeath);
	}
}

void UVeyraShopSubsystem::OnDeath(const FVeyraDeathEvent& Death)
{
	AActor* Participant = VeyraQuests::LaneFluxbornLastHitter(Death);
	UVeyraInventoryComponent* Inventory = Participant ? Participant->FindComponentByClass<UVeyraInventoryComponent>() : nullptr;
	if (!Inventory || !Participant->HasAuthority())
	{
		return;
	}
	const FVeyraItemsTuning& Tuning = UVeyraItemsTuningSubsystem::Get();
	TArray<FVeyraInventorySlot> Slots = Inventory->Slots;
	const TArray<FVeyraContentId> Evolved = VeyraQuests::Advance(Tuning, Slots, EVeyraQuestObjective::LaneFluxbornLastHits);
	if (Slots == Inventory->Slots)
	{
		return;
	}
	Inventory->SetSlots(MoveTemp(Slots));
	for (const FVeyraContentId& Item : Evolved)
	{
		UE_LOG(LogVeyraItems, Log, TEXT("%s's quest is complete: it holds %s."), *GetNameSafe(Participant), *Item.ToString());
	}
	if (!Evolved.IsEmpty())
	{
		ApplyItems(*Participant);
	}
}

void UVeyraShopSubsystem::OnHostileDamage(const FVeyraHostileDamageEvent& Event)
{
	const UAbilitySystemComponent* Source = Event.Source.Get();
	const UAbilitySystemComponent* Target = Event.Target.Get();
	AActor* Participant = Source ? Source->GetOwner() : nullptr;
	UVeyraInventoryComponent* Inventory = Participant ? Participant->FindComponentByClass<UVeyraInventoryComponent>() : nullptr;
	if (!Inventory || !Target || !VeyraUnits::IsVanguard(Target->GetOwner()) || !Participant->HasAuthority())
	{
		return;
	}
	const bool bBasicAttack = Event.Delivery == EVeyraDamageDelivery::BasicAttack;
	const bool bAbility = Event.Delivery == EVeyraDamageDelivery::Ability;
	const FVeyraItemsTuning& Tuning = UVeyraItemsTuningSubsystem::Get();
	const double Now = GetWorld()->GetTimeSeconds();
	bool bChanged = false;
	for (const FVeyraInventorySlot& Slot : Inventory->Slots)
	{
		const FVeyraItemDefinition* Item = Slot.IsEmpty() ? nullptr : Tuning.Items.Find(Slot.Item);
		if (!Item)
		{
			continue;
		}
		for (const FVeyraContentId& Attunement : Item->Attunement)
		{
			const FVeyraStackingAttunementTuning* SpoolUp = bBasicAttack ? Tuning.SpoolUp.Find(Attunement) : nullptr;
			const FVeyraStackingAttunementTuning* Overcycle = bAbility ? Tuning.Overcycle.Find(Attunement) : nullptr;
			const FVeyraStackingAttunementTuning* Stacking = SpoolUp ? SpoolUp : Overcycle;
			if (!Stacking)
			{
				continue;
			}
			// Each qualifying hit adds a stack up to the cap and refreshes them all (Item Bible §8–§9).
			FVeyraAttunementStacks& Held = Inventory->Stacks.FindOrAdd(Attunement);
			Held.Count = FMath::Min(Held.Count + 1, Stacking->MaxStacks);
			Held.ExpiresAt = Now + Stacking->DurationSeconds;
			bChanged = true;
		}
	}
	if (!bChanged)
	{
		return;
	}
	Stacked.AddUnique(Participant);
	ApplyItems(*Participant);
	if (!GetWorld()->GetTimerManager().IsTimerActive(StackTimer))
	{
		GetWorld()->GetTimerManager().SetTimer(StackTimer, FTimerDelegate::CreateUObject(this, &UVeyraShopSubsystem::ExpireStacks),
			static_cast<float>(UVeyraCombatTuningSubsystem::Get().Regeneration.TickSeconds), /*bLoop*/ true);
	}
}

void UVeyraShopSubsystem::ExpireStacks()
{
	const double Now = GetWorld()->GetTimeSeconds();
	for (int32 Index = Stacked.Num() - 1; Index >= 0; --Index)
	{
		AActor* Participant = Stacked[Index].Get();
		UVeyraInventoryComponent* Inventory = Participant ? Participant->FindComponentByClass<UVeyraInventoryComponent>() : nullptr;
		if (!Inventory)
		{
			Stacked.RemoveAt(Index);
			continue;
		}
		bool bExpired = false;
		for (auto It = Inventory->Stacks.CreateIterator(); It; ++It)
		{
			if (It->Value.ExpiresAt <= Now)
			{
				It.RemoveCurrent();
				bExpired = true;
			}
		}
		if (bExpired)
		{
			ApplyItems(*Participant);
		}
		if (Inventory->Stacks.IsEmpty())
		{
			Stacked.RemoveAt(Index);
		}
	}
	if (Stacked.IsEmpty())
	{
		GetWorld()->GetTimerManager().ClearTimer(StackTimer);
	}
}

void UVeyraShopSubsystem::InitializeInventory(AActor& Participant)
{
	if (UVeyraInventoryComponent* Inventory = Participant.FindComponentByClass<UVeyraInventoryComponent>())
	{
		TArray<FVeyraInventorySlot> Slots;
		Slots.SetNum(UVeyraItemsTuningSubsystem::Get().Shop.InventorySlots);
		Inventory->SetSlots(MoveTemp(Slots));
		Inventory->SetMythical(FVeyraContentId());
	}
}

void UVeyraShopSubsystem::ApplyItems(AActor& Participant)
{
	const UVeyraInventoryComponent* Inventory = Participant.FindComponentByClass<UVeyraInventoryComponent>();
	UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Participant);
	// The dead get nothing from their items, whatever they buy, sell or undo, until they respawn
	// (ADR-012 §9): Match applies them then.
	if (!Inventory || !AbilitySystem || !VeyraTargeting::IsAlive(&Participant))
	{
		return;
	}
	// Bonus Attack Speed is a fraction of the base, beside level growth, so the two add (ADR-012 §6).
	const UVeyraProgressionComponent* Progression = Participant.FindComponentByClass<UVeyraProgressionComponent>();
	const FVeyraItemsTuning& Tuning = UVeyraItemsTuningSubsystem::Get();
	const FVeyraEquipmentStats Stats = VeyraEquipment::StatsFor(Tuning, Inventory->Slots, Progression ? Progression->GetBaseAttackSpeed() : 0.0,
		Inventory->GetStackCounts());
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
	// A Vanguard in Stasis takes no action, and drinking is one: its restoration would outlast the Stasis (ADR-050 §1).
	if (const UAbilitySystemComponent* Drinker = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Participant); Drinker && VeyraCombat::IsInStasis(*Drinker))
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
	if (Consumable->Charges > 0)
	{
		// A refillable one spends a charge and stays (Item Bible §12).
		if (Slots[Index].Charges <= 0)
		{
			return EVeyraShopRefusal::NoCharges;
		}
		--Slots[Index].Charges;
		Slots[Index].bBenefited = true;
	}
	else if (--Slots[Index].Count == 0)
	{
		Slots[Index] = FVeyraInventorySlot();
	}
	Inventory->SetSlots(MoveTemp(Slots));
	// A used consumable has given benefit; no undo may bring it back (§12).
	Inventory->ResetUndoSteps();

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

void UVeyraShopSubsystem::RefillCharges(AActor& Participant)
{
	UVeyraInventoryComponent* Inventory = Participant.FindComponentByClass<UVeyraInventoryComponent>();
	if (!Inventory)
	{
		return;
	}
	const FVeyraItemsTuning& Tuning = UVeyraItemsTuningSubsystem::Get();
	TArray<FVeyraInventorySlot> Slots = Inventory->Slots;
	bool bRefilled = false;
	for (FVeyraInventorySlot& Slot : Slots)
	{
		const FVeyraConsumableTuning* Consumable = Slot.IsEmpty() ? nullptr : Tuning.Consumables.Find(Slot.Item);
		if (Consumable && Consumable->Charges > Slot.Charges)
		{
			Slot.Charges = Consumable->Charges;
			bRefilled = true;
		}
	}
	if (bRefilled)
	{
		Inventory->SetSlots(MoveTemp(Slots));
		UE_LOG(LogVeyraItems, Log, TEXT("%s refilled its charges."), *GetNameSafe(&Participant));
	}
}

void UVeyraShopSubsystem::GrowHealth(AActor& Participant, const FVeyraContentId& Attunement, double Health)
{
	UVeyraInventoryComponent* Inventory = Participant.FindComponentByClass<UVeyraInventoryComponent>();
	if (!Inventory || !(Health > 0.0))
	{
		return;
	}
	const FVeyraItemsTuning& Tuning = UVeyraItemsTuningSubsystem::Get();
	TArray<FVeyraInventorySlot> Slots = Inventory->Slots;
	FVeyraInventorySlot* Holding = Slots.FindByPredicate([&Tuning, &Attunement](const FVeyraInventorySlot& Slot) {
		const FVeyraItemDefinition* Item = Slot.IsEmpty() ? nullptr : Tuning.Items.Find(Slot.Item);
		return Item && Item->Attunement.Contains(Attunement);
	});
	if (!Holding)
	{
		return;
	}
	Holding->GrownHealth += Health;
	// It has given benefit: no undo takes it back (§12).
	Holding->bBenefited = true;
	Inventory->SetSlots(MoveTemp(Slots));
	ApplyItems(Participant);
	UE_LOG(LogVeyraItems, Log, TEXT("%s's %s grew %g Max Health."), *GetNameSafe(&Participant), *Attunement.ToString(), Health);
}

void UVeyraShopSubsystem::SetStored(AActor& Participant, const FVeyraContentId& Attunement, EVeyraItemStore Store, double Amount)
{
	UVeyraInventoryComponent* Inventory = Participant.FindComponentByClass<UVeyraInventoryComponent>();
	if (!Inventory || !FMath::IsFinite(Amount))
	{
		return;
	}
	const FVeyraItemsTuning& Tuning = UVeyraItemsTuningSubsystem::Get();
	TArray<FVeyraInventorySlot> Slots = Inventory->Slots;
	FVeyraInventorySlot* Holding = Slots.FindByPredicate([&Tuning, &Attunement](const FVeyraInventorySlot& Slot) {
		const FVeyraItemDefinition* Item = Slot.IsEmpty() ? nullptr : Tuning.Items.Find(Slot.Item);
		return Item && Item->Attunement.Contains(Attunement);
	});
	const double Stored = FMath::Max(0.0, Amount);
	if (!Holding || Holding->Stored(Store) == Stored)
	{
		return;
	}
	// Spending it has given benefit: no undo takes the item back (§12).
	Holding->bBenefited |= Stored < Holding->Stored(Store);
	Holding->Stored(Store) = Stored;
	Inventory->SetSlots(MoveTemp(Slots));
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
	Inventory->ResetUndoSteps();
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
		// The participant heals itself with its item (ADR-017 §9).
		VeyraCombat::RestoreHealthFrom(*AbilitySystem, *AbilitySystem, Restoration.PerTick);
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
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RestorationTimer);
		World->GetTimerManager().ClearTimer(StackTimer);
		if (UVeyraCombatEventSubsystem* Events = World->GetSubsystem<UVeyraCombatEventSubsystem>())
		{
			Events->OnHostileDamage.Remove(HostileDamageHandle);
			Events->OnDeath.Remove(DeathHandle);
		}
	}
	Restorations.Reset();
	Stacked.Reset();
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
	// The Mythical as each purchase found it: none before the one that chose it, if it waits here.
	const bool bChoosesMythical = Inventory->Queue.ContainsByPredicate([](const FVeyraPendingPurchase& Entry) { return Entry.bSetsMythical; });
	FVeyraContentId Mythical = bChoosesMythical ? FVeyraContentId() : Inventory->Mythical;
	for (const FVeyraPendingPurchase& Entry : Inventory->Queue)
	{
		// Each delivered purchase can be undone at the fountain like one bought there (§12).
		FVeyraUndoStep Step;
		Step.Paid = Entry.Paid;
		Step.SlotsBefore = Slots;
		Step.MythicalBefore = Mythical;
		Mythical = Entry.bSetsMythical ? Entry.Item : Mythical;
		const EVeyraShopRefusal Refusal = VeyraInventory::Apply(Tuning, Slots, Entry);
		check(Refusal == EVeyraShopRefusal::None);
		Gold->SettleHold(Entry.GoldHold);
		Inventory->AddUndoStep(MoveTemp(Step));
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
		// The first invalid entry; replaying again shows what depended on it. A Mythical it chose is released.
		Gold.ReleaseHold(Queue[Invalid[0]].GoldHold);
		if (Queue[Invalid[0]].bSetsMythical)
		{
			Inventory.SetMythical(FVeyraContentId());
		}
		Queue.RemoveAt(Invalid[0]);
		bChanged = true;
	}
	if (bChanged)
	{
		Inventory.SetQueue(MoveTemp(Queue));
	}
}
