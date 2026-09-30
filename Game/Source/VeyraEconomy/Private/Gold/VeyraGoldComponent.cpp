// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Gold/VeyraGoldComponent.h"

#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Rewards/VeyraEconomyTuningSubsystem.h"
#include "Rewards/VeyraRewardRules.h"
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
	case EVeyraGoldReason::Sale:
		return TEXT("a sale");
	case EVeyraGoldReason::Undo:
		return TEXT("an undone purchase");
	case EVeyraGoldReason::Passive:
		return TEXT("passive income");
	case EVeyraGoldReason::Wildlife:
		return TEXT("a jungle creature");
	case EVeyraGoldReason::FluxWell:
		return TEXT("a share of a Flux Well");
	case EVeyraGoldReason::WardDestroyed:
		return TEXT("a ward destroyed");
	case EVeyraGoldReason::Bounty:
		return TEXT("a bounty");
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
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraGoldComponent, Holds, Params);

	FDoRepLifetimeParams Public;
	Public.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraGoldComponent, KillStreak, Public);
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraGoldComponent, DeathStreak, Public);
}

double UVeyraGoldComponent::GetBounty() const
{
	return VeyraRewards::Bounty(KillStreak, UVeyraEconomyTuningSubsystem::Get().Bounty);
}

void UVeyraGoldComponent::SetStreaks(int32 NewKillStreak, int32 NewDeathStreak)
{
	if (NewKillStreak != KillStreak)
	{
		KillStreak = NewKillStreak;
		MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraGoldComponent, KillStreak, this);
	}
	if (NewDeathStreak != DeathStreak)
	{
		DeathStreak = NewDeathStreak;
		MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraGoldComponent, DeathStreak, this);
	}
}

bool UVeyraGoldComponent::Grant(double Amount, EVeyraGoldReason Reason)
{
	check(GetOwner() && GetOwner()->HasAuthority());
	if (!FMath::IsFinite(Amount) || !(Amount > 0.0))
	{
		return false;
	}
	SetGold(Gold + Amount);
	UE_LOG(LogVeyraEconomy, Verbose, TEXT("%s gained %.2f Gold for %s, now %.2f."), *GetNameSafe(GetOwner()), Amount, LexToString(Reason), Gold);
	OnGoldGranted.Broadcast(Amount, Reason);
	return true;
}

bool UVeyraGoldComponent::Spend(double Amount)
{
	check(GetOwner() && GetOwner()->HasAuthority());
	if (!FMath::IsFinite(Amount) || Amount < 0.0 || Amount > Gold)
	{
		return false;
	}
	SetGold(Gold - Amount);
	UE_LOG(LogVeyraEconomy, Verbose, TEXT("%s spent %.2f Gold, now %.2f."), *GetNameSafe(GetOwner()), Amount, Gold);
	return true;
}

TOptional<int32> UVeyraGoldComponent::Hold(double Amount)
{
	if (!Spend(Amount))
	{
		return {};
	}
	FVeyraGoldHold& Held = Holds.AddDefaulted_GetRef();
	Held.Id = NextHoldId++;
	Held.Amount = Amount;
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraGoldComponent, Holds, this);
	return Held.Id;
}

double UVeyraGoldComponent::ReleaseHold(int32 Id)
{
	check(GetOwner() && GetOwner()->HasAuthority());
	const int32 Index = Holds.IndexOfByPredicate([Id](const FVeyraGoldHold& Held) { return Held.Id == Id; });
	if (Index == INDEX_NONE)
	{
		return 0.0;
	}
	const double Amount = Holds[Index].Amount;
	Holds.RemoveAt(Index);
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraGoldComponent, Holds, this);
	SetGold(Gold + Amount);
	UE_LOG(LogVeyraEconomy, Verbose, TEXT("%s had %.2f held Gold back from a cancelled purchase, now %.2f."), *GetNameSafe(GetOwner()), Amount, Gold);
	return Amount;
}

void UVeyraGoldComponent::SettleHold(int32 Id)
{
	check(GetOwner() && GetOwner()->HasAuthority());
	if (Holds.RemoveAll([Id](const FVeyraGoldHold& Held) { return Held.Id == Id; }) > 0)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraGoldComponent, Holds, this);
	}
}

void UVeyraGoldComponent::SetGold(double NewGold)
{
	Gold = NewGold;
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraGoldComponent, Gold, this);
}
