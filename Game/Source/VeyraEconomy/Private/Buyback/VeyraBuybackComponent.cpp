// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Buyback/VeyraBuybackComponent.h"

#include "Gold/VeyraGoldComponent.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Rewards/VeyraEconomyTuningSubsystem.h"
#include "VeyraEconomyLog.h"

UVeyraBuybackComponent::UVeyraBuybackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UVeyraBuybackComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.Condition = COND_OwnerOnly;
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraBuybackComponent, Purchases, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraBuybackComponent, ReadyAt, Params);
}

FVeyraBuybackQuote UVeyraBuybackComponent::Quote(double MatchSeconds, double Now, bool bDead, const UVeyraGoldComponent& Gold) const
{
	return VeyraBuyback::Quote(MatchSeconds, Now, bDead, Gold.GetGold(), Purchases, ReadyAt, UVeyraEconomyTuningSubsystem::Get().Buyback);
}

EVeyraBuybackRefusal UVeyraBuybackComponent::Buy(double MatchSeconds, double Now, bool bDead, UVeyraGoldComponent& Gold)
{
	check(GetOwner() && GetOwner()->HasAuthority());
	const FVeyraBuybackQuote Next = Quote(MatchSeconds, Now, bDead, Gold);
	if (Next.Refusal != EVeyraBuybackRefusal::None)
	{
		return Next.Refusal;
	}
	if (!Gold.Spend(Next.Cost))
	{
		return EVeyraBuybackRefusal::NotEnoughGold;
	}
	++Purchases;
	ReadyAt = Now + UVeyraEconomyTuningSubsystem::Get().Buyback.CooldownSeconds;
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraBuybackComponent, Purchases, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraBuybackComponent, ReadyAt, this);
	UE_LOG(LogVeyraEconomy, Log, TEXT("%s bought back for %.2f Gold (buyback %d); the next is ready at %.1f s."), *GetNameSafe(GetOwner()), Next.Cost, Purchases,
		ReadyAt);
	OnBoughtBack.Broadcast(Next.Cost);
	return EVeyraBuybackRefusal::None;
}
