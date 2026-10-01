// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Loadout/VeyraFollowUpSubsystem.h"

#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "VeyraAbilitiesVerbs.h"

void UVeyraFollowUpSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UVeyraCombatEventSubsystem* Events = Collection.InitializeDependency<UVeyraCombatEventSubsystem>())
	{
		MarkerEndHandle = Events->OnMarkerEnded.AddUObject(this, &UVeyraFollowUpSubsystem::OnMarkerEnded);
	}
}

void UVeyraFollowUpSubsystem::Deinitialize()
{
	UWorld* World = GetWorld();
	if (UVeyraCombatEventSubsystem* Events = World ? World->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		Events->OnMarkerEnded.Remove(MarkerEndHandle);
	}
	for (FArming& Each : Arming)
	{
		if (World)
		{
			World->GetTimerManager().ClearTimer(Each.Timer);
		}
	}
	Arming.Reset();
	Super::Deinitialize();
}

void UVeyraFollowUpSubsystem::OpenAfter(UAbilitySystemComponent& Caster, EVeyraAbilitySlot Slot, const FVeyraOverrideSpec& FollowUp, double Seconds)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FArming& Added = Arming.AddDefaulted_GetRef();
	Added.Key = NextKey++;
	Added.Caster = &Caster;
	Added.Slot = Slot;
	Added.FollowUp = FollowUp;
	World->GetTimerManager().SetTimer(Added.Timer, FTimerDelegate::CreateUObject(this, &UVeyraFollowUpSubsystem::Open, Added.Key), static_cast<float>(Seconds),
		/*bLoop*/ false);
}

void UVeyraFollowUpSubsystem::Open(int32 Key)
{
	const int32 Index = Arming.IndexOfByPredicate([Key](const FArming& Each) { return Each.Key == Key; });
	if (Index == INDEX_NONE)
	{
		return;
	}
	const FArming Armed = Arming[Index];
	Arming.RemoveAt(Index);
	UAbilitySystemComponent* Caster = Armed.Caster.Get();
	const AActor* Participant = Caster ? Caster->GetOwner() : nullptr;
	if (UVeyraAbilityLoadoutComponent* Loadout = Participant ? Participant->FindComponentByClass<UVeyraAbilityLoadoutComponent>() : nullptr)
	{
		Loadout->Override(*Caster, Armed.Slot, Armed.FollowUp);
	}
}

void UVeyraFollowUpSubsystem::OnMarkerEnded(const FVeyraMarkerEnd& End)
{
	// The follow-up its marker's ability opened has nothing left to act on (ADR-032 §5).
	if (UAbilitySystemComponent* Owner = End.Owner.Get())
	{
		VeyraAbilities::EndFollowUp(*Owner, End.Id);
	}
}
