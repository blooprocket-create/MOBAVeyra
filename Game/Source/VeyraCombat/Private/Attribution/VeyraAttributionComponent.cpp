// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attribution/VeyraAttributionComponent.h"

#include "AbilitySystemComponent.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"

UVeyraAttributionComponent::UVeyraAttributionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVeyraAttributionComponent::NoteContribution(UAbilitySystemComponent& Source, double Now)
{
	LastContributions.Add(&Source, Now);
}

TArray<UAbilitySystemComponent*> UVeyraAttributionComponent::GetAssisters(const UAbilitySystemComponent* Killer, double Now) const
{
	const double Window = UVeyraCombatTuningSubsystem::Get().Attribution.AssistWindowSeconds;
	TArray<UAbilitySystemComponent*> Assisters;
	for (const TPair<TWeakObjectPtr<UAbilitySystemComponent>, double>& Contribution : LastContributions)
	{
		UAbilitySystemComponent* Contributor = Contribution.Key.Get();
		if (Contributor && Contributor != Killer && Now - Contribution.Value <= Window)
		{
			Assisters.Add(Contributor);
		}
	}
	return Assisters;
}

TArray<FVeyraContribution> UVeyraAttributionComponent::GetContributions() const
{
	TArray<FVeyraContribution> Contributions;
	for (const TPair<TWeakObjectPtr<UAbilitySystemComponent>, double>& Contribution : LastContributions)
	{
		if (Contribution.Key.IsValid())
		{
			Contributions.Add({ Contribution.Key, Contribution.Value });
		}
	}
	return Contributions;
}

void UVeyraAttributionComponent::Clear()
{
	LastContributions.Reset();
}
