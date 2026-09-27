// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Movement/VeyraMovementComponent.h"

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraMobilitySet.h"
#include "Movement/VeyraMovementRules.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"

void UVeyraMovementComponent::BindCombatant(UAbilitySystemComponent* Combatant)
{
	if (UVeyraStatusComponent* Statuses = FollowedStatuses.Get())
	{
		Statuses->OnStatusesChanged.Remove(StatusesChangedHandle);
	}
	StatusesChangedHandle.Reset();
	FollowedCombatant = Combatant;
	FollowedStatuses = nullptr;

	AActor* Participant = Combatant ? Combatant->GetOwner() : nullptr;
	if (UVeyraStatusComponent* Statuses = Participant ? Participant->FindComponentByClass<UVeyraStatusComponent>() : nullptr)
	{
		FollowedStatuses = Statuses;
		StatusesChangedHandle = Statuses->OnStatusesChanged.AddUObject(this, &UVeyraMovementComponent::RefreshMovementLock);
	}
	RefreshMovementLock();
}

float UVeyraMovementComponent::GetMaxSpeed() const
{
	const UAbilitySystemComponent* Combatant = FollowedCombatant.Get();
	const bool bWalks = MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking || MovementMode == MOVE_Falling;
	if (!Combatant || !bWalks)
	{
		return Super::GetMaxSpeed();
	}

	const UVeyraStatusComponent* Statuses = FollowedStatuses.Get();
	FVeyraSpeedInputs Inputs;
	Inputs.MoveSpeed = Combatant->GetNumericAttribute(UVeyraMobilitySet::GetMoveSpeedAttribute());
	Inputs.BaseMoveSpeed = Combatant->GetNumericAttributeBase(UVeyraMobilitySet::GetMoveSpeedAttribute());
	Inputs.StrongestSlow = Statuses ? Statuses->GetStrongestSlow() : 0.0;
	Inputs.bStunned = Statuses && EnumHasAnyFlags(Statuses->GetActionBlocks(), EVeyraActionBlocks::Move);
	return static_cast<float>(VeyraMovementRules::EffectiveSpeed(Inputs, UVeyraCombatTuningSubsystem::Get().Movement));
}

void UVeyraMovementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	BindCombatant(nullptr);
	Super::EndPlay(EndPlayReason);
}

void UVeyraMovementComponent::RefreshMovementLock()
{
	const UVeyraStatusComponent* Statuses = FollowedStatuses.Get();
	const bool bLocked = Statuses && EnumHasAnyFlags(Statuses->GetActionBlocks(), EVeyraActionBlocks::Move);
	if (bLocked == bMovementLocked)
	{
		return;
	}
	bMovementLocked = bLocked;
	if (bLocked)
	{
		// A locked unit stops where it stands rather than braking to a halt (Combat Bible §8). Its
		// path is its controller's to keep or drop, so stopping here leaves the path alone.
		StopMovementKeepPathing();
	}
	OnMovementLockChanged.Broadcast(bLocked);
}
