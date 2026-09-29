// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Inventory/VeyraInventoryComponent.h"

#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

UVeyraInventoryComponent::UVeyraInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UVeyraInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Everyone;
	Everyone.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraInventoryComponent, Slots, Everyone);

	FDoRepLifetimeParams Owner;
	Owner.bIsPushBased = true;
	Owner.Condition = COND_OwnerOnly;
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraInventoryComponent, Queue, Owner);
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraInventoryComponent, bAtFountain, Owner);
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraInventoryComponent, UndoStepCount, Owner);
}

TMap<FVeyraContentId, int32> UVeyraInventoryComponent::GetStackCounts() const
{
	TMap<FVeyraContentId, int32> Counts;
	for (const TPair<FVeyraContentId, FVeyraAttunementStacks>& Held : Stacks)
	{
		Counts.Add(Held.Key, Held.Value.Count);
	}
	return Counts;
}

void UVeyraInventoryComponent::SetSlots(TArray<FVeyraInventorySlot> NewSlots)
{
	Slots = MoveTemp(NewSlots);
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraInventoryComponent, Slots, this);
}

void UVeyraInventoryComponent::SetQueue(TArray<FVeyraPendingPurchase> NewQueue)
{
	Queue = MoveTemp(NewQueue);
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraInventoryComponent, Queue, this);
}

void UVeyraInventoryComponent::SetAtFountainState(bool bNewAtFountain)
{
	bAtFountain = bNewAtFountain;
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraInventoryComponent, bAtFountain, this);
}

void UVeyraInventoryComponent::AddUndoStep(FVeyraUndoStep Step)
{
	UndoSteps.Add(MoveTemp(Step));
	SetUndoStepCount();
}

void UVeyraInventoryComponent::PopUndoStep()
{
	UndoSteps.Pop();
	SetUndoStepCount();
}

void UVeyraInventoryComponent::ResetUndoSteps()
{
	UndoSteps.Reset();
	SetUndoStepCount();
}

void UVeyraInventoryComponent::SetUndoStepCount()
{
	if (UndoStepCount != UndoSteps.Num())
	{
		UndoStepCount = UndoSteps.Num();
		MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraInventoryComponent, UndoStepCount, this);
	}
}
