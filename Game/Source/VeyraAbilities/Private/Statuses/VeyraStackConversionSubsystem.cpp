// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Statuses/VeyraStackConversionSubsystem.h"

#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraCombatVerbs.h"

void UVeyraStackConversionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UVeyraCombatEventSubsystem* Events = Collection.InitializeDependency<UVeyraCombatEventSubsystem>())
	{
		StatusAppliedHandle = Events->OnStatusApplied.AddUObject(this, &UVeyraStackConversionSubsystem::OnStatusApplied);
	}
}

void UVeyraStackConversionSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (UVeyraCombatEventSubsystem* Events = World->GetSubsystem<UVeyraCombatEventSubsystem>())
		{
			Events->OnStatusApplied.Remove(StatusAppliedHandle);
		}
	}
	Super::Deinitialize();
}

void UVeyraStackConversionSubsystem::OnStatusApplied(const FVeyraStatusApplied& Applied)
{
	UAbilitySystemComponent* Source = Applied.Source.Get();
	UAbilitySystemComponent* Target = Applied.Target.Get();
	AActor* TargetOwner = Target ? Target->GetOwner() : nullptr;
	const FVeyraStatusTuning* Tuning = UVeyraAbilitiesTuningSubsystem::Get().Statuses.Find(Applied.Id);
	const UVeyraStatusComponent* Ledger = TargetOwner ? TargetOwner->FindComponentByClass<UVeyraStatusComponent>() : nullptr;
	if (!Source || !Ledger || !TargetOwner->HasAuthority() || !Tuning || Tuning->AtMaxStacks.IsEmpty())
	{
		return;
	}
	int32 Stacks = 0;
	for (const FVeyraStatusEntry& Entry : Ledger->GetLedger().Entries)
	{
		Stacks += Entry.Id == Applied.Id ? Entry.Stacks : 0;
	}
	if (Stacks < Tuning->MaxStacks)
	{
		return;
	}
	// At its most, it becomes the other status, from the same source (ADR-026 §2).
	VeyraCombat::RemoveStatus(*Target, Applied.Id);
	if (const TOptional<FVeyraStatusSpec> Next = UVeyraAbilitiesTuningSubsystem::FindStatus(Tuning->AtMaxStacks[0]))
	{
		VeyraCombat::ApplyStatus(*Source, *Target, Next.GetValue());
	}
}
