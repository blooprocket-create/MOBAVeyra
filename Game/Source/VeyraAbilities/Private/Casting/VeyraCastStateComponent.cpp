// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Casting/VeyraCastStateComponent.h"

#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

UVeyraCastStateComponent::UVeyraCastStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UVeyraCastStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraCastStateComponent, State, Params);
}

void UVeyraCastStateComponent::SetState(const FVeyraCastState& NewState)
{
	State = NewState;
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraCastStateComponent, State, this);
	OnCastStateChanged.Broadcast();
}

void UVeyraCastStateComponent::Clear()
{
	SetState(FVeyraCastState());
}

void UVeyraCastStateComponent::OnRep_State()
{
	OnCastStateChanged.Broadcast();
}
