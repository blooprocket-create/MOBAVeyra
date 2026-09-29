// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Inventory/VeyraInventoryRules.h"

#include "VeyraInventoryComponent.generated.h"

/** A stacking Attunement's stacks (Item Bible §8–§9: Spool Up, Overcycle). Server only. */
struct FVeyraAttunementStacks
{
	int32 Count = 0;

	/** When they all fall away, in the server's world time; each qualifying hit refreshes it. */
	double ExpiresAt = 0.0;
};

/** One purchase made at the fountain that undo can take back (Economy & Progression Bible §12). Server only. */
struct FVeyraUndoStep
{
	/** What the purchase cost, which undo returns whole. */
	double Paid = 0.0;

	/** The slots before it. */
	TArray<FVeyraInventorySlot> SlotsBefore;
};

/**
 * One participant's items (Economy & Progression Bible §10–§12; ADR-012 §5): its slots, which every
 * machine sees as a scoreboard does, and the purchases waiting for the fountain, which only its owner
 * sees. It sits on the PlayerState, so items survive death. Only UVeyraShopSubsystem changes it.
 */
UCLASS()
class VEYRAITEMS_API UVeyraInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVeyraInventoryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	const TArray<FVeyraInventorySlot>& GetSlots() const { return Slots; }

	/** Owner and server only: purchases paid for and not yet delivered, in order. */
	const TArray<FVeyraPendingPurchase>& GetQueue() const { return Queue; }

	/**
	 * Owner and server only: whether the shop delivers, sells and undoes for the participant now: at
	 * its own fountain, or dead, as Match reports it (ADR-012 §7, §9).
	 */
	bool IsAtFountain() const { return bAtFountain; }

	/** Server only: this fountain visit's purchases that undo can take back, oldest first. */
	const TArray<FVeyraUndoStep>& GetUndoSteps() const { return UndoSteps; }

	/** Owner and server only: how many purchases undo can take back now, for the shop screen. */
	int32 GetUndoStepCount() const { return UndoStepCount; }

	/** Server only: the stacks each stacking Attunement holds now, by Attunement. */
	TMap<FVeyraContentId, int32> GetStackCounts() const;

private:
	friend class UVeyraShopSubsystem;

	void SetSlots(TArray<FVeyraInventorySlot> NewSlots);
	void SetQueue(TArray<FVeyraPendingPurchase> NewQueue);
	void SetAtFountainState(bool bNewAtFountain);

	// The undo steps change only through these, which keep their replicated count.
	void AddUndoStep(FVeyraUndoStep Step);
	void PopUndoStep();
	void ResetUndoSteps();
	void SetUndoStepCount();

	UPROPERTY(Replicated)
	TArray<FVeyraInventorySlot> Slots;

	UPROPERTY(Replicated)
	TArray<FVeyraPendingPurchase> Queue;

	UPROPERTY(Replicated)
	bool bAtFountain = false;

	UPROPERTY(Replicated)
	int32 UndoStepCount = 0;

	TArray<FVeyraUndoStep> UndoSteps;
	TMap<FVeyraContentId, FVeyraAttunementStacks> Stacks;
};
