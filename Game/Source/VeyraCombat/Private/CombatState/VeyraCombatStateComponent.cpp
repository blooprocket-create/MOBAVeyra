// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CombatState/VeyraCombatStateComponent.h"

#include "Engine/World.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"
#include "Targeting/VeyraParticipantData.h"

UVeyraCombatStateComponent::UVeyraCombatStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

ELifetimeCondition UVeyraCombatStateComponent::GetReplicationCondition() const
{
	return VeyraParticipantData::ConditionFor(*this, Super::GetReplicationCondition());
}

void UVeyraCombatStateComponent::ReadyForReplication()
{
	Super::ReadyForReplication();
	if (VeyraParticipantData::IsParticipantData(*this) && GetOwner()->HasAuthority())
	{
		VeyraParticipantData::Gate(*this, *GetOwner());
	}
}

void UVeyraCombatStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// A participant's reaches those who see it; a unit's, those its body reaches (ADR-016 §3).
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraCombatStateComponent, bInCombat, Params);
}

void UVeyraCombatStateComponent::NoteCombat()
{
	UWorld* World = GetWorld();
	if (!World || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	// Each fight restarts the delay. A world timer, so a pause holds it (ADR-006 §8).
	World->GetTimerManager().SetTimer(OutOfCombatTimer, FTimerDelegate::CreateUObject(this, &UVeyraCombatStateComponent::SetInCombat, false),
		static_cast<float>(UVeyraCombatTuningSubsystem::Get().CombatState.OutOfCombatSeconds), /*bLoop*/ false);
	SetInCombat(true);
}

void UVeyraCombatStateComponent::Clear()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(OutOfCombatTimer);
	}
	SetInCombat(false);
}

void UVeyraCombatStateComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(OutOfCombatTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void UVeyraCombatStateComponent::OnRep_InCombat()
{
	OnCombatStateChanged.Broadcast(bInCombat);
}

void UVeyraCombatStateComponent::SetInCombat(bool bNewInCombat)
{
	if (bInCombat == bNewInCombat)
	{
		return;
	}
	bInCombat = bNewInCombat;
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraCombatStateComponent, bInCombat, this);
	OnCombatStateChanged.Broadcast(bInCombat);
}
