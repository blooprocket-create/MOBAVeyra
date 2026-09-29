// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Loadout/VeyraAbilityLoadoutComponent.h"

#include "AbilitySystemComponent.h"
#include "Abilities/VeyraAreaAbility.h"
#include "Abilities/VeyraDashAbility.h"
#include "Abilities/VeyraEmpoweredAttackAbility.h"
#include "Abilities/VeyraSelfBuffAbility.h"
#include "Abilities/VeyraSkillshotAbility.h"
#include "Abilities/VeyraTargetedDamageAbility.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraAbilitiesLog.h"

namespace
{
	// An ability's rank comes from Progression, not from the Gameplay Ability System's level, so
	// every ability is granted at the system's default level and its numbers come from tuning.
	constexpr int32 DefaultAbilityLevel = 1;

	/** The archetype class that runs Ability, from the map the Abilities tuning defines it in (ADR-008 §3). */
	TSubclassOf<UVeyraGameplayAbility> ArchetypeFor(const FVeyraContentId& Ability)
	{
		if (UVeyraAbilitiesTuningSubsystem::FindTargetedDamage(Ability))
		{
			return UVeyraTargetedDamageAbility::StaticClass();
		}
		if (UVeyraAbilitiesTuningSubsystem::FindArea(Ability))
		{
			return UVeyraAreaAbility::StaticClass();
		}
		if (UVeyraAbilitiesTuningSubsystem::FindSelfBuff(Ability))
		{
			return UVeyraSelfBuffAbility::StaticClass();
		}
		if (UVeyraAbilitiesTuningSubsystem::FindSkillshot(Ability))
		{
			return UVeyraSkillshotAbility::StaticClass();
		}
		if (UVeyraAbilitiesTuningSubsystem::FindDash(Ability))
		{
			return UVeyraDashAbility::StaticClass();
		}
		if (UVeyraAbilitiesTuningSubsystem::FindEmpoweredAttack(Ability))
		{
			return UVeyraEmpoweredAttackAbility::StaticClass();
		}
		return nullptr;
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
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraAbilityLoadoutComponent, UnlockedSpellSlots, Params);
}

void UVeyraAbilityLoadoutComponent::SetUnlockedSpellSlots(int32 Count)
{
	check(GetOwner() && GetOwner()->HasAuthority());
	// Permanent Flux never falls, so neither do its unlocks (Battleground Bible §14).
	const int32 Clamped = FMath::Clamp(Count, UnlockedSpellSlots, static_cast<int32>(UE_ARRAY_COUNT(VeyraAbilitySlots::Spells)));
	if (Clamped != UnlockedSpellSlots)
	{
		UnlockedSpellSlots = Clamped;
		MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraAbilityLoadoutComponent, UnlockedSpellSlots, this);
	}
}

bool UVeyraAbilityLoadoutComponent::IsLocked(EVeyraAbilitySlot Slot) const
{
	return VeyraAbilitySlots::IsSpellSlot(Slot) && VeyraAbilitySlots::SpellIndexOf(Slot) >= UnlockedSpellSlots;
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
	Entry->Handle = AbilitySystem.GiveAbility(FGameplayAbilitySpec(Archetype, DefaultAbilityLevel));
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraAbilityLoadoutComponent, Entries, this);
	return Entry->Handle.IsValid();
}

void UVeyraAbilityLoadoutComponent::Clear(UAbilitySystemComponent& AbilitySystem, EVeyraAbilitySlot Slot)
{
	check(GetOwner() && GetOwner()->HasAuthority());
	const int32 Index = Entries.IndexOfByPredicate([Slot](const FVeyraLoadoutEntry& Candidate) { return Candidate.Slot == Slot; });
	if (Index == INDEX_NONE)
	{
		return;
	}
	AbilitySystem.ClearAbility(Entries[Index].Handle);
	Entries.RemoveAt(Index);
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraAbilityLoadoutComponent, Entries, this);
}

const FVeyraLoadoutEntry* UVeyraAbilityLoadoutComponent::FindSlot(EVeyraAbilitySlot Slot) const
{
	return Entries.FindByPredicate([Slot](const FVeyraLoadoutEntry& Candidate) { return Candidate.Slot == Slot; });
}

const FVeyraLoadoutEntry* UVeyraAbilityLoadoutComponent::FindAbility(const FVeyraContentId& Ability) const
{
	return Entries.FindByPredicate([&Ability](const FVeyraLoadoutEntry& Candidate) { return Candidate.Ability == Ability; });
}

const FVeyraLoadoutEntry* UVeyraAbilityLoadoutComponent::FindHandle(FGameplayAbilitySpecHandle Handle) const
{
	return Entries.FindByPredicate([Handle](const FVeyraLoadoutEntry& Candidate) { return Candidate.Handle == Handle; });
}
