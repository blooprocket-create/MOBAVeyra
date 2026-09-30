// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Engine/TimerHandle.h"
#include "Inventory/VeyraInventoryRules.h"
#include "Subsystems/WorldSubsystem.h"

#include "VeyraShopSubsystem.generated.h"

class AActor;
class UVeyraGoldComponent;
class UVeyraInventoryComponent;

/** What the key of an item slot does (author ruling 2026-09-28; ADR-012 §1). */
enum class EVeyraItemUse : uint8
{
	/** The slot is empty, or its item has nothing to use. */
	None,
	/** Used up: the shop uses it. */
	Consumable,
	/** An Active: Abilities casts it from the item slot, under the match's cast rules. */
	Active,
};

/**
 * The shop's transactions (Economy & Progression Bible §10–§12; ADR-012 §5): buying, cancelling,
 * selling, undoing and delivering, each applied at once to a participant's Gold, through Economy, and
 * items, through its UVeyraInventoryComponent, and then to its stats through Combat. The rules are
 * VeyraInventory's; this decides only when they apply. A participant is its PlayerState, which holds
 * its inventory, Gold, progression and Ability System Component. Server only; Match routes the
 * fountain, deaths and the players' requests here.
 */
UCLASS()
class VEYRAITEMS_API UVeyraShopSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Buys Item for Participant (§10–§11). At its fountain, or while dead, it is delivered at once and
	 * can be undone there; elsewhere its Gold is held and it waits in the queue.
	 */
	EVeyraShopRefusal Buy(AActor& Participant, const FVeyraContentId& Item);

	/**
	 * Puts Item in Participant's inventory now and for no Gold, wherever it stands, by a purchase's
	 * slot, limit and recipe rules, so the owned items its recipe needs are consumed. Nothing is left
	 * to undo. The developer command Veyra.Dev.GiveItem uses it.
	 */
	EVeyraShopRefusal GrantItem(AActor& Participant, const FVeyraContentId& Item);

	/** Cancels the pending purchase at Index, from anywhere, for all its Gold, with whatever needed it (§11.3). */
	EVeyraShopRefusal Cancel(AActor& Participant, int32 Index);

	/** Sells one of the item in Slot at the fountain for its resale value (§12). */
	EVeyraShopRefusal Sell(AActor& Participant, int32 Slot);

	/** Undoes this fountain visit's latest purchase for all it cost, if what it made gave no benefit yet (§12). */
	EVeyraShopRefusal Undo(AActor& Participant);

	/**
	 * Puts roster spell Spell in Participant's Flux Spell slot Slot (from 0) for Economy.json's swap
	 * cost, at its fountain only and never queued (Battleground Bible §14; Economy & Progression Bible
	 * §13.2; ADR-015 §6). A locked slot may be swapped, and stays locked. If the spell it replaces is
	 * cooling down, the new one starts on its full cooldown, so a swap never resets one.
	 */
	EVeyraShopRefusal SwapFluxSpell(AActor& Participant, int32 Slot, const FVeyraContentId& Spell);

	/**
	 * Takes Cost Gold from Participant for a change the shop does not own, such as the vision tool
	 * (ADR-016 §6), at its fountain only, or dead (ADR-012 §9). Match makes the change once it is paid.
	 */
	EVeyraShopRefusal ChargeAtFountain(AActor& Participant, double Cost, const TCHAR* ForWhat);

	/**
	 * Match reports Participant arriving at or leaving its own fountain. Arriving delivers the queue
	 * (§11.2); leaving ends undo (§12).
	 */
	void SetAtFountain(AActor& Participant, bool bAtFountain);

	/** Match reports Participant's death: its queue is delivered, to use once it respawns (§11.2). */
	void DeliverOnDeath(AActor& Participant);

	/**
	 * Fills Participant's refillable consumables to their charges (Item Bible §12; ADR-023 §6): its
	 * arrival at its fountain does, and Match calls it for each of a side's participants when the
	 * side secures a Flux Well.
	 */
	void RefillCharges(AActor& Participant);

	/**
	 * Grows the Max Health of Participant's item that holds Attunement by Health, for as long as it
	 * holds the item (Tempered by Conflict; ADR-023 §3). The Attunements call it.
	 */
	void GrowHealth(AActor& Participant, const FVeyraContentId& Attunement, double Health);

	/**
	 * Sets what Participant's item holding Attunement stores, its Current or its Reserve (ADR-025 §7),
	 * at least 0. Spending it is benefit, which ends undo. The Attunements call it.
	 */
	void SetStored(AActor& Participant, const FVeyraContentId& Attunement, EVeyraItemStore Store, double Amount);

	/**
	 * A unit died: a last hit on an enemy lane Fluxborn advances the last hitter's quests, which evolve
	 * at their thresholds (Item Bible §10; ADR-025 §3). Server only. Public for tests; Combat's event
	 * calls it in a match.
	 */
	void OnDeath(const struct FVeyraDeathEvent& Death);

	/** Sets Participant's slot count, empty, as its match prepares. */
	static void InitializeInventory(AActor& Participant);

	/**
	 * After any change to Participant's slots: what its items add to its stats (ADR-012 §6), and the
	 * Actives its item slots hold, granted in the slot each item sits in and cleared from empty ones.
	 */
	static void ApplyItems(AActor& Participant);

	/** What the key of inventory slot Index, from 0, does for Participant now. */
	static EVeyraItemUse GetUse(const AActor& Participant, int32 Index);

	/**
	 * Uses one of the consumable in inventory slot Index (Item Bible §12): Field Tonic restores its
	 * Health over its duration. Refused while dead, or while one is still restoring.
	 */
	EVeyraShopRefusal UseConsumable(AActor& Participant, int32 Index);

	/** Participant cast the Active in inventory slot Index: the item has given benefit, so undo ends (§12). */
	static void NoteActiveUsed(AActor& Participant, int32 Index);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Hostile damage landed: a stacking Attunement of the source's items gains a stack when its hit
	 * qualifies (Item Bible §8–§9): Spool Up from a basic attack, Overcycle from an ability, each on an
	 * enemy Vanguard. Server only. Public for tests; Combat's event calls it in a match.
	 */
	void OnHostileDamage(const struct FVeyraHostileDamageEvent& Event);

	/** Server only: drops every stack whose time is up and reapplies what changed. Its timer calls it. */
	void ExpireStacks();

private:
	/** A consumable restoring Health over time. */
	struct FRestoration
	{
		TWeakObjectPtr<AActor> Participant;
		double PerTick = 0.0;
		int32 TicksLeft = 0;
	};

	void OnRestorationTimer();

	TArray<FRestoration> Restorations;
	FTimerHandle RestorationTimer;

	/** The participants holding stacks, which the stack timer checks. */
	TArray<TWeakObjectPtr<AActor>> Stacked;
	FTimerHandle StackTimer;
	FDelegateHandle HostileDamageHandle;
	FDelegateHandle DeathHandle;

	/** Whether Participant may receive, sell and undo now: at its fountain, or dead (ADR-012 §9). */
	static bool IsAtShop(const AActor& Participant, const UVeyraInventoryComponent& Inventory);

	/** Delivers every valid pending purchase in order, settling its Gold; refunds the invalid ones. */
	void Deliver(AActor& Participant);

	/** Cancels, with full refunds, every pending purchase that can no longer deliver (§11.3). */
	void Revalidate(UVeyraInventoryComponent& Inventory, UVeyraGoldComponent& Gold);
};
