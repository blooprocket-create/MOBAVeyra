// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Inventory/VeyraInventoryRules.h"
#include "Subsystems/WorldSubsystem.h"

#include "VeyraShopSubsystem.generated.h"

class AActor;
class UVeyraGoldComponent;
class UVeyraInventoryComponent;

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

	/** Cancels the pending purchase at Index, from anywhere, for all its Gold, with whatever needed it (§11.3). */
	EVeyraShopRefusal Cancel(AActor& Participant, int32 Index);

	/** Sells one of the item in Slot at the fountain for its resale value (§12). */
	EVeyraShopRefusal Sell(AActor& Participant, int32 Slot);

	/** Undoes this fountain visit's latest purchase for all it cost, if what it made gave no benefit yet (§12). */
	EVeyraShopRefusal Undo(AActor& Participant);

	/**
	 * Match reports Participant arriving at or leaving its own fountain. Arriving delivers the queue
	 * (§11.2); leaving ends undo (§12).
	 */
	void SetAtFountain(AActor& Participant, bool bAtFountain);

	/** Match reports Participant's death: its queue is delivered, to use once it respawns (§11.2). */
	void DeliverOnDeath(AActor& Participant);

	/** Sets Participant's slot count, empty, as its match prepares. */
	static void InitializeInventory(AActor& Participant);

	/** Recomputes what Participant's items add to its stats (ADR-012 §6). */
	static void ApplyEquipment(AActor& Participant);

private:
	/** Whether Participant may receive, sell and undo now: at its fountain, or dead (ADR-012 §9). */
	static bool IsAtShop(const AActor& Participant, const UVeyraInventoryComponent& Inventory);

	/** Delivers every valid pending purchase in order, settling its Gold; refunds the invalid ones. */
	void Deliver(AActor& Participant);

	/** Cancels, with full refunds, every pending purchase that can no longer deliver (§11.3). */
	void Revalidate(UVeyraInventoryComponent& Inventory, UVeyraGoldComponent& Gold);
};
