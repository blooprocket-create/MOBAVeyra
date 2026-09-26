// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Loadout/VeyraAbilityLoadoutComponent.h"

#include "AbilitySystemComponent.h"
#include "Abilities/VeyraTargetedDamageAbility.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraAbilitiesLog.h"

namespace
{
	// Ability ranks (skill points) arrive with progression; until then every ability is granted at
	// the Gameplay Ability System's first level, and its numbers come from tuning, not levels.
	constexpr int32 UnrankedAbilityLevel = 1;

	/** The archetype class that runs Ability, from where the Abilities tuning defines it. */
	TSubclassOf<UVeyraGameplayAbility> ArchetypeFor(const FVeyraContentId& Ability)
	{
		return UVeyraAbilitiesTuningSubsystem::FindTargetedDamage(Ability) ? UVeyraTargetedDamageAbility::StaticClass() : nullptr;
	}
}

UVeyraAbilityLoadoutComponent::UVeyraAbilityLoadoutComponent()
{
	SetIsReplicatedByDefault(true);
}

void UVeyraAbilityLoadoutComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.Condition = COND_ReplayOrOwner;
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraAbilityLoadoutComponent, Entries, Params);
}

bool UVeyraAbilityLoadoutComponent::Grant(UAbilitySystemComponent& AbilitySystem, EVeyraAbilitySlot Slot, const FVeyraContentId& Ability)
{
	check(GetOwner() && GetOwner()->HasAuthority());
	const TSubclassOf<UVeyraGameplayAbility> Archetype = ArchetypeFor(Ability);
	if (!Archetype)
	{
		UE_LOG(LogVeyraAbilities, Error, TEXT("Cannot grant %s to %s: the Abilities tuning defines no ability with that ID."),
			*Ability.ToString(), *GetNameSafe(GetOwner()));
		return false;
	}

	FVeyraLoadoutEntry* Entry = Entries.FindByPredicate([Slot](const FVeyraLoadoutEntry& Candidate) { return Candidate.Slot == Slot; });
	if (Entry)
	{
		AbilitySystem.ClearAbility(Entry->Handle);
	}
	else
	{
		Entry = &Entries.AddDefaulted_GetRef();
		Entry->Slot = Slot;
	}
	Entry->Ability = Ability;
	Entry->Handle = AbilitySystem.GiveAbility(FGameplayAbilitySpec(Archetype, UnrankedAbilityLevel));
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraAbilityLoadoutComponent, Entries, this);
	return Entry->Handle.IsValid();
}

const FVeyraLoadoutEntry* UVeyraAbilityLoadoutComponent::FindSlot(EVeyraAbilitySlot Slot) const
{
	return Entries.FindByPredicate([Slot](const FVeyraLoadoutEntry& Candidate) { return Candidate.Slot == Slot; });
}

const FVeyraLoadoutEntry* UVeyraAbilityLoadoutComponent::FindHandle(FGameplayAbilitySpecHandle Handle) const
{
	return Entries.FindByPredicate([Handle](const FVeyraLoadoutEntry& Candidate) { return Candidate.Handle == Handle; });
}
