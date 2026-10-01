// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Delivery/VeyraShieldHoldSubsystem.h"

#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Targeting/VeyraTargeting.h"
#include "VeyraAbilitiesLog.h"
#include "VeyraCombatVerbs.h"

void UVeyraShieldHoldSubsystem::Deinitialize()
{
	for (const FHold& Each : Holds)
	{
		Unbind(Each);
	}
	Holds.Reset();
	Super::Deinitialize();
}

void UVeyraShieldHoldSubsystem::Unbind(const FHold& Hold)
{
	UAbilitySystemComponent* Holder = Hold.Holder.Get();
	if (FOnActiveGameplayEffectRemoved_Info* Removed = Holder ? Holder->OnGameplayEffectRemoved_InfoDelegate(Hold.Shield) : nullptr)
	{
		Removed->Remove(Hold.Removed);
	}
}

void UVeyraShieldHoldSubsystem::Release(const UAbilitySystemComponent& Provider, const UAbilitySystemComponent& Holder, const FVeyraContentId& Id)
{
	Holds.RemoveAll([&Provider, &Holder, &Id](const FHold& Each) {
		const bool bTakenOver = Each.Provider.Get() == &Provider && Each.Holder.Get() == &Holder && Each.Id == Id;
		if (bTakenOver)
		{
			Unbind(Each);
		}
		return bTakenOver;
	});
}

void UVeyraShieldHoldSubsystem::Hold(UAbilitySystemComponent& Provider, UAbilitySystemComponent& Holder, const FActiveGameplayEffectHandle& Shield,
	const FVeyraContentId& Id, TConstArrayView<FVeyraStatusSpec> Statuses, TArray<FVeyraPreparedZone> EndZones, const FVeyraAbilityHitSource& Source)
{
	FOnActiveGameplayEffectRemoved_Info* Removed = Holder.OnGameplayEffectRemoved_InfoDelegate(Shield);
	if (!Removed)
	{
		UE_LOG(LogVeyraAbilities, Warning, TEXT("%s's shield %s on %s ended before it could be watched."), *GetNameSafe(Provider.GetOwner()), *Id.ToString(),
			*GetNameSafe(Holder.GetOwner()));
		return;
	}
	FHold& Added = Holds.AddDefaulted_GetRef();
	Added.Key = NextKey++;
	Added.Provider = &Provider;
	Added.Holder = &Holder;
	Added.Shield = Shield;
	Added.Id = Id;
	Added.EndZones = MoveTemp(EndZones);
	Added.Source = Source;
	for (const FVeyraStatusSpec& Status : Statuses)
	{
		Added.Statuses.Add(Status.Id);
		VeyraCombat::ApplyStatus(Provider, Holder, Status);
	}
	Added.Removed = Removed->AddUObject(this, &UVeyraShieldHoldSubsystem::OnShieldRemoved, Added.Key);
}

void UVeyraShieldHoldSubsystem::OnShieldRemoved(const FGameplayEffectRemovalInfo& /*Removal*/, int32 Key)
{
	const int32 Index = Holds.IndexOfByPredicate([Key](const FHold& Each) { return Each.Key == Key; });
	if (Index == INDEX_NONE)
	{
		return;
	}
	// GAS is still removing the effect: its delegate goes with it, so only the watch is dropped here.
	const FHold Ended = MoveTemp(Holds[Index]);
	Holds.RemoveAt(Index);
	UAbilitySystemComponent* Holder = Ended.Holder.Get();
	UAbilitySystemComponent* Provider = Ended.Provider.Get();
	if (!Holder)
	{
		return;
	}
	for (const FVeyraContentId& Status : Ended.Statuses)
	{
		VeyraCombat::RemoveStatus(*Holder, Status);
	}
	// It bursts where its holder stands, the provider's; a holder that has died keeps none (ADR-032 §3).
	const AActor* Body = Holder->GetAvatarActor();
	UWorld* World = GetWorld();
	if (Ended.EndZones.IsEmpty() || !Provider || !Body || !World || !VeyraTargeting::IsAlive(Body))
	{
		return;
	}
	FVeyraEffectFrame Frame;
	Frame.Origin = Body->GetActorLocation();
	Frame.Direction = Body->GetActorForwardVector().GetSafeNormal2D();
	VeyraAreaDelivery::Resolve(*World, *Provider, Frame, Ended.EndZones, Ended.Source);
	UE_LOG(LogVeyraAbilities, Verbose, TEXT("%s's shield %s on %s ended and burst."), *GetNameSafe(Provider->GetOwner()), *Ended.Id.ToString(), *GetNameSafe(Body));
}
