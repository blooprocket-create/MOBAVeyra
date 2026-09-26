// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attributes/VeyraMobilitySet.h"

#include "Net/UnrealNetwork.h"

UVeyraMobilitySet::UVeyraMobilitySet()
{
	// A unit does not move until its data gives it a speed.
	InitMoveSpeed(0.0f);
}

void UVeyraMobilitySet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.RepNotifyCondition = REPNOTIFY_Always;
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraMobilitySet, MoveSpeed, Params);
}

void UVeyraMobilitySet::OnRep_MoveSpeed(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVeyraMobilitySet, MoveSpeed, OldValue);
}
