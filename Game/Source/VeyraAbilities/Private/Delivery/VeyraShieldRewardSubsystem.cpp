// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Delivery/VeyraShieldRewardSubsystem.h"

#include "Absorption/VeyraAbsorptionLedger.h"
#include "AbilitySystemComponent.h"
#include "Delivery/VeyraEffectDelivery.h"
#include "Engine/World.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Tuning/VeyraAbilitiesTuning.h"
#include "VeyraCombatVerbs.h"

void UVeyraShieldRewardSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UVeyraCombatEventSubsystem* Events = Collection.InitializeDependency<UVeyraCombatEventSubsystem>())
	{
		ResolvedHandle = Events->OnDamageResolved.AddUObject(this, &UVeyraShieldRewardSubsystem::OnDamageResolved);
	}
}

void UVeyraShieldRewardSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (UVeyraCombatEventSubsystem* Events = World->GetSubsystem<UVeyraCombatEventSubsystem>())
		{
			Events->OnDamageResolved.Remove(ResolvedHandle);
		}
	}
	Watches.Reset();
	Super::Deinitialize();
}

void UVeyraShieldRewardSubsystem::Watch(UAbilitySystemComponent& Provider, UAbilitySystemComponent& Holder, const FVeyraShieldGrant& Grant, const FVeyraAbsorbedRewardTuning& Reward)
{
	// A new grant of the same shield starts the count again.
	Watches.RemoveAll([&Provider, &Holder, &Grant](const FWatch& Each) {
		return Each.Provider.Get() == &Provider && Each.Holder.Get() == &Holder && Each.Id == Grant.Id;
	});
	FWatch& Added = Watches.AddDefaulted_GetRef();
	Added.Provider = &Provider;
	Added.Holder = &Holder;
	Added.Id = Grant.Id;
	Added.Needed = Grant.Amount * Reward.Fraction;
	Added.Until = GetWorld()->GetTimeSeconds() + Grant.DurationSeconds;
	Added.Statuses = VeyraEffectDelivery::StatusSpecs(Reward.Statuses);
}

void UVeyraShieldRewardSubsystem::OnDamageResolved(const FVeyraDamageResolution& Resolution)
{
	const UWorld* World = GetWorld();
	if (Watches.IsEmpty() || !World)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	Watches.RemoveAll([Now](const FWatch& Each) { return Each.Until < Now || !Each.Provider.IsValid() || !Each.Holder.IsValid(); });
	for (int32 Index = Watches.Num() - 1; Index >= 0; --Index)
	{
		FWatch& Each = Watches[Index];
		if (Resolution.Target.Get() != Each.Holder.Get())
		{
			continue;
		}
		for (const FVeyraShieldShare& Share : Resolution.Shields)
		{
			Each.Absorbed += Share.Provider.Get() == Each.Provider.Get() && Share.Id == Each.Id ? Share.Absorbed : 0.0;
		}
		if (Each.Absorbed >= Each.Needed)
		{
			// Once: its statuses, from the shield's provider, then the watch ends.
			const FWatch Rewarded = MoveTemp(Each);
			Watches.RemoveAt(Index);
			for (const FVeyraStatusSpec& Status : Rewarded.Statuses)
			{
				VeyraCombat::ApplyStatus(*Rewarded.Provider.Get(), *Rewarded.Holder.Get(), Status);
			}
		}
	}
}
