// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Statistics/VeyraScoreComponent.h"

#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

UVeyraScoreComponent::UVeyraScoreComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UVeyraScoreComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Everyone's, seen or not: never participant data behind the fog (ADR-016 §3).
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraScoreComponent, Score, Params);
}

void UVeyraScoreComponent::SetScore(const FVeyraScore& NewScore)
{
	check(GetOwner() && GetOwner()->HasAuthority());
	if (Score == NewScore)
	{
		return;
	}
	Score = NewScore;
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraScoreComponent, Score, this);
}
