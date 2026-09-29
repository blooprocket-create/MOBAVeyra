// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Gold/VeyraGoldComponent.h"

#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "VeyraEconomyLog.h"

const TCHAR* LexToString(EVeyraGoldReason Reason)
{
	switch (Reason)
	{
	case EVeyraGoldReason::Starting:
		return TEXT("starting Gold");
	case EVeyraGoldReason::LastHit:
		return TEXT("a last hit");
	case EVeyraGoldReason::Participation:
		return TEXT("a share of a nearby Fluxborn");
	case EVeyraGoldReason::Kill:
		return TEXT("a kill");
	case EVeyraGoldReason::Assist:
		return TEXT("an assist");
	case EVeyraGoldReason::FirstBlood:
		return TEXT("First Blood");
	case EVeyraGoldReason::StructurePool:
		return TEXT("a share of a structure");
	case EVeyraGoldReason::FirstStructure:
		return TEXT("the first structure");
	case EVeyraGoldReason::Developer:
		return TEXT("a developer grant");
	}
	return TEXT("unknown");
}

UVeyraGoldComponent::UVeyraGoldComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UVeyraGoldComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.Condition = COND_OwnerOnly;
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraGoldComponent, Gold, Params);
}

bool UVeyraGoldComponent::Grant(double Amount, EVeyraGoldReason Reason)
{
	check(GetOwner() && GetOwner()->HasAuthority());
	if (!FMath::IsFinite(Amount) || !(Amount > 0.0))
	{
		return false;
	}
	Gold += Amount;
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraGoldComponent, Gold, this);
	UE_LOG(LogVeyraEconomy, Verbose, TEXT("%s gained %.2f Gold for %s, now %.2f."), *GetNameSafe(GetOwner()), Amount, LexToString(Reason), Gold);
	return true;
}
