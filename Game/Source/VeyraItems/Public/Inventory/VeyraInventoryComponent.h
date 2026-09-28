// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Inventory/VeyraInventoryRules.h"

#include "VeyraInventoryComponent.generated.h"

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

	/** Server only: whether the Vanguard stands at its own fountain, as Match reports it. */
	bool IsAtFountain() const { return bAtFountain; }

	/** Server only: this fountain visit's purchases that undo can take back, oldest first. */
	const TArray<FVeyraUndoStep>& GetUndoSteps() const { return UndoSteps; }

private:
	friend class UVeyraShopSubsystem;

	void SetSlots(TArray<FVeyraInventorySlot> NewSlots);
	void SetQueue(TArray<FVeyraPendingPurchase> NewQueue);

	UPROPERTY(Replicated)
	TArray<FVeyraInventorySlot> Slots;

	UPROPERTY(Replicated)
	TArray<FVeyraPendingPurchase> Queue;

	bool bAtFountain = false;
	TArray<FVeyraUndoStep> UndoSteps;
};
