// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Life/VeyraLifeComponent.h"

#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

UVeyraLifeComponent::UVeyraLifeComponent()
{
	SetIsReplicatedByDefault(true);
}

void UVeyraLifeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraLifeComponent, State, Params);
}

bool UVeyraLifeComponent::SetState(EVeyraLifeState NewState)
{
	check(GetOwner() && GetOwner()->HasAuthority());
	if (State == NewState)
	{
		return false;
	}
	State = NewState;
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraLifeComponent, State, this);
	return true;
}
