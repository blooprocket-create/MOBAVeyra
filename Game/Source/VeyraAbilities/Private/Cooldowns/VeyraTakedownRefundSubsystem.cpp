// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Cooldowns/VeyraTakedownRefundSubsystem.h"

#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Life/VeyraKillCredit.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraAbilitiesLog.h"
#include "VeyraAbilitiesVerbs.h"

void UVeyraTakedownRefundSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UVeyraCombatEventSubsystem* Events = Collection.InitializeDependency<UVeyraCombatEventSubsystem>())
	{
		DeathHandle = Events->OnDeath.AddUObject(this, &UVeyraTakedownRefundSubsystem::OnDeath);
	}
}

void UVeyraTakedownRefundSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (UVeyraCombatEventSubsystem* Events = World->GetSubsystem<UVeyraCombatEventSubsystem>())
		{
			Events->OnDeath.Remove(DeathHandle);
		}
	}
	Super::Deinitialize();
}

void UVeyraTakedownRefundSubsystem::OnDeath(const FVeyraDeathEvent& Death)
{
	const FVeyraAbilitiesTuning& Tuning = UVeyraAbilitiesTuningSubsystem::Get();
	for (UAbilitySystemComponent* Participant : VeyraKillCredit::TakedownParticipants(Death))
	{
		const AActor* Owner = Participant ? Participant->GetOwner() : nullptr;
		const UVeyraAbilityLoadoutComponent* Loadout = Owner ? Owner->FindComponentByClass<UVeyraAbilityLoadoutComponent>() : nullptr;
		if (!Loadout)
		{
			continue;
		}
		// Collected first: a refund changes nothing in the loadout, but the walk stays read-only.
		TArray<TPair<FVeyraContentId, double>, TInlineAllocator<4>> Refunds;
		Loadout->ForEachAbility([&Tuning, &Refunds](const FVeyraLoadoutEntry& Entry) {
			const FVeyraCastTuning* Cast = VeyraAbilityRules::FindCast(Tuning, Entry.Ability);
			if (Cast && !Cast->TakedownRefund.IsEmpty())
			{
				Refunds.Emplace(Entry.Ability, Cast->TakedownRefund[0]);
			}
		});
		for (const TPair<FVeyraContentId, double>& Refund : Refunds)
		{
			VeyraAbilities::RefundCooldown(*Participant, Refund.Key, Refund.Value);
			UE_LOG(LogVeyraAbilities, Verbose, TEXT("%s's takedown refunds %g of %s's cooldown."), *GetNameSafe(Owner), Refund.Value, *Refund.Key.ToString());
		}
	}
}
