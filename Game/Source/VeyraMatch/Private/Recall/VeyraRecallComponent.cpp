// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Recall/VeyraRecallComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Statuses/VeyraStatusComponent.h"
#include "TimerManager.h"
#include "VeyraMatchLog.h"
#include "Targeting/VeyraParticipantData.h"

UVeyraRecallComponent::UVeyraRecallComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

ELifetimeCondition UVeyraRecallComponent::GetReplicationCondition() const
{
	return VeyraParticipantData::ConditionFor(*this, Super::GetReplicationCondition());
}

void UVeyraRecallComponent::ReadyForReplication()
{
	Super::ReadyForReplication();
	if (VeyraParticipantData::IsParticipantData(*this) && GetOwner()->HasAuthority())
	{
		VeyraParticipantData::Gate(*this, *GetOwner());
	}
}

void UVeyraRecallComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraRecallComponent, Channel, Params);
}

void UVeyraRecallComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(Timer);
	}
	Watch(false);
	OnComplete.Unbind();
	Super::EndPlay(EndPlayReason);
}

void UVeyraRecallComponent::Start(double Seconds, FSimpleDelegate Complete)
{
	check(GetOwner() && GetOwner()->HasAuthority());
	UWorld* World = GetWorld();
	OnComplete = MoveTemp(Complete);
	const double Now = World->GetTimeSeconds();
	SetChannel(Now, Now + Seconds);
	World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &UVeyraRecallComponent::OnChannelComplete), static_cast<float>(Seconds),
		/*bLoop*/ false);
	Watch(true);
	UE_LOG(LogVeyraMatch, Verbose, TEXT("%s begins recalling (%g s)."), *GetNameSafe(GetOwner()), Seconds);
}

bool UVeyraRecallComponent::Interrupt()
{
	if (!IsRecalling())
	{
		return false;
	}
	GetWorld()->GetTimerManager().ClearTimer(Timer);
	Watch(false);
	OnComplete.Unbind();
	SetChannel(0.0, 0.0);
	UE_LOG(LogVeyraMatch, Verbose, TEXT("%s's Recall was interrupted."), *GetNameSafe(GetOwner()));
	return true;
}

void UVeyraRecallComponent::OnChannelComplete()
{
	Watch(false);
	SetChannel(0.0, 0.0);
	const FSimpleDelegate Complete = MoveTemp(OnComplete);
	OnComplete.Unbind();
	Complete.ExecuteIfBound();
}

void UVeyraRecallComponent::SetChannel(double StartedAt, double EndsAt)
{
	Channel.StartedAt = StartedAt;
	Channel.EndsAt = EndsAt;
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraRecallComponent, Channel, this);
}

void UVeyraRecallComponent::Watch(bool bWatch)
{
	UWorld* World = GetWorld();
	UVeyraCombatEventSubsystem* Events = World ? World->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr;
	UVeyraStatusComponent* Statuses = GetOwner() ? GetOwner()->FindComponentByClass<UVeyraStatusComponent>() : nullptr;
	if (Events)
	{
		Events->OnHostileDamage.Remove(HostileDamageHandle);
		Events->OnDeath.Remove(DeathHandle);
	}
	if (Statuses)
	{
		Statuses->OnInterrupted.Remove(InterruptedHandle);
	}
	HostileDamageHandle.Reset();
	DeathHandle.Reset();
	InterruptedHandle.Reset();
	if (!bWatch)
	{
		return;
	}
	if (Events)
	{
		HostileDamageHandle = Events->OnHostileDamage.AddUObject(this, &UVeyraRecallComponent::OnHostileDamage);
		DeathHandle = Events->OnDeath.AddUObject(this, &UVeyraRecallComponent::OnDeath);
	}
	if (Statuses)
	{
		InterruptedHandle = Statuses->OnInterrupted.AddUObject(this, &UVeyraRecallComponent::OnInterrupted);
	}
}

const UAbilitySystemComponent* UVeyraRecallComponent::GetAbilitySystem() const
{
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
}

void UVeyraRecallComponent::OnHostileDamage(const FVeyraHostileDamageEvent& Event)
{
	if (Event.Target.Get() == GetAbilitySystem())
	{
		Interrupt();
	}
}

void UVeyraRecallComponent::OnDeath(const FVeyraDeathEvent& Death)
{
	if (Death.Victim.Get() == GetAbilitySystem())
	{
		Interrupt();
	}
}

void UVeyraRecallComponent::OnInterrupted()
{
	Interrupt();
}
