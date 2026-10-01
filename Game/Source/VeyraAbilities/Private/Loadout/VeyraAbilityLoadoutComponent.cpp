// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Loadout/VeyraAbilityLoadoutComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/VeyraAreaAbility.h"
#include "Abilities/VeyraAmbushAbility.h"
#include "Abilities/VeyraAttachAbility.h"
#include "Abilities/VeyraDashAbility.h"
#include "Abilities/VeyraRideAbility.h"
#include "Abilities/VeyraEmpoweredAttackAbility.h"
#include "Abilities/VeyraSelfBuffAbility.h"
#include "Abilities/VeyraSkillshotAbility.h"
#include "Abilities/VeyraStanceAbility.h"
#include "Abilities/VeyraTargetedDamageAbility.h"
#include "Abilities/VeyraTetherAbility.h"
#include "Abilities/VeyraVolleyAbility.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraAbilitiesLog.h"
#include "VeyraAbilitiesVerbs.h"

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
		if (UVeyraAbilitiesTuningSubsystem::FindVolley(Ability))
		{
			return UVeyraVolleyAbility::StaticClass();
		}
		if (UVeyraAbilitiesTuningSubsystem::FindTether(Ability))
		{
			return UVeyraTetherAbility::StaticClass();
		}
		if (UVeyraAbilitiesTuningSubsystem::FindAttach(Ability))
		{
			return UVeyraAttachAbility::StaticClass();
		}
		if (UVeyraAbilitiesTuningSubsystem::FindRide(Ability))
		{
			return UVeyraRideAbility::StaticClass();
		}
		if (UVeyraAbilitiesTuningSubsystem::FindAmbush(Ability))
		{
			return UVeyraAmbushAbility::StaticClass();
		}
		if (UVeyraAbilitiesTuningSubsystem::FindStance(Ability))
		{
			return UVeyraStanceAbility::StaticClass();
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
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraAbilityLoadoutComponent, Overrides, Params);
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
	EndOverride(AbilitySystem, Slot);
	AbilitySystem.ClearAbility(Entries[Index].Handle);
	Entries.RemoveAt(Index);
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraAbilityLoadoutComponent, Entries, this);
	for (int32 StowedIndex = Stowed.Num() - 1; StowedIndex >= 0; --StowedIndex)
	{
		if (Stowed[StowedIndex].Slot == Slot)
		{
			AbilitySystem.ClearAbility(Stowed[StowedIndex].Handle);
			Stowed.RemoveAt(StowedIndex);
		}
	}
}

bool UVeyraAbilityLoadoutComponent::SwapOwn(UAbilitySystemComponent& AbilitySystem, EVeyraAbilitySlot Slot, const FVeyraContentId& Ability)
{
	check(GetOwner() && GetOwner()->HasAuthority());
	FVeyraLoadoutEntry* Own = Entries.FindByPredicate([Slot](const FVeyraLoadoutEntry& Candidate) { return Candidate.Slot == Slot; });
	if (!Own)
	{
		return false;
	}
	if (Own->Ability == Ability)
	{
		return true;
	}
	FVeyraLoadoutEntry Incoming;
	const int32 StowedIndex = Stowed.IndexOfByPredicate([Slot, &Ability](const FVeyraLoadoutEntry& Candidate) { return Candidate.Slot == Slot && Candidate.Ability == Ability; });
	if (StowedIndex != INDEX_NONE)
	{
		// It comes back with the grant it kept; its cooldown never left (ADR-031 §3).
		Incoming = Stowed[StowedIndex];
		Stowed.RemoveAt(StowedIndex);
	}
	else
	{
		const TSubclassOf<UVeyraGameplayAbility> Archetype = ArchetypeFor(Ability);
		if (!Archetype)
		{
			UE_LOG(LogVeyraAbilities, Error, TEXT("Cannot put %s in %s's slot: the Abilities tuning defines no ability with that ID."), *Ability.ToString(),
				*GetNameSafe(GetOwner()));
			return false;
		}
		Incoming.Slot = Slot;
		Incoming.Ability = Ability;
		Incoming.Handle = AbilitySystem.GiveAbility(FGameplayAbilitySpec(Archetype, DefaultAbilityLevel));
	}
	// Stowed with its grant, not taken back: a cast of it still running finishes.
	Stowed.Add(*Own);
	*Own = Incoming;
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraAbilityLoadoutComponent, Entries, this);
	// An override that belongs to one own ability shows or waits with it.
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraAbilityLoadoutComponent, Overrides, this);
	UE_LOG(LogVeyraAbilities, Verbose, TEXT("%s's slot holds %s as its own, %s stowed."), *GetNameSafe(GetOwner()), *Ability.ToString(),
		*Stowed.Last().Ability.ToString());
	return Own->Handle.IsValid();
}

const FVeyraLoadoutEntry* UVeyraAbilityLoadoutComponent::FindStowed(EVeyraAbilitySlot Slot) const
{
	return Stowed.FindByPredicate([Slot](const FVeyraLoadoutEntry& Candidate) { return Candidate.Slot == Slot; });
}

bool UVeyraAbilityLoadoutComponent::IsShown(const FVeyraSlotOverride& Override) const
{
	if (!Override.Over.IsValid())
	{
		return true;
	}
	const EVeyraAbilitySlot Slot = Override.Entry.Slot;
	const FVeyraLoadoutEntry* Own = Entries.FindByPredicate([Slot](const FVeyraLoadoutEntry& Candidate) { return Candidate.Slot == Slot; });
	return Own && Own->Ability == Override.Over;
}

const FVeyraLoadoutEntry* UVeyraAbilityLoadoutComponent::FindSlot(EVeyraAbilitySlot Slot) const
{
	const FVeyraSlotOverride* Override = Overrides.FindByPredicate([Slot](const FVeyraSlotOverride& Candidate) { return Candidate.Entry.Slot == Slot; });
	if (Override && IsShown(*Override))
	{
		return &Override->Entry;
	}
	return Entries.FindByPredicate([Slot](const FVeyraLoadoutEntry& Candidate) { return Candidate.Slot == Slot; });
}

const FVeyraLoadoutEntry* UVeyraAbilityLoadoutComponent::FindOwnSlot(EVeyraAbilitySlot Slot) const
{
	return Entries.FindByPredicate([Slot](const FVeyraLoadoutEntry& Entry) { return Entry.Slot == Slot; });
}

const FVeyraSlotOverride* UVeyraAbilityLoadoutComponent::FindOverride(EVeyraAbilitySlot Slot) const
{
	return Overrides.FindByPredicate([Slot](const FVeyraSlotOverride& Override) { return Override.Entry.Slot == Slot; });
}

const FVeyraLoadoutEntry* UVeyraAbilityLoadoutComponent::FindAbility(const FVeyraContentId& Ability) const
{
	if (const FVeyraLoadoutEntry* Own = Entries.FindByPredicate([&Ability](const FVeyraLoadoutEntry& Candidate) { return Candidate.Ability == Ability; }))
	{
		return Own;
	}
	if (const FVeyraSlotOverride* Override = Overrides.FindByPredicate([&Ability](const FVeyraSlotOverride& Candidate) { return Candidate.Entry.Ability == Ability; }))
	{
		return &Override->Entry;
	}
	if (const FVeyraLoadoutEntry* Put = Stowed.FindByPredicate([&Ability](const FVeyraLoadoutEntry& Candidate) { return Candidate.Ability == Ability; }))
	{
		return Put;
	}
	return Retired.FindByPredicate([&Ability](const FVeyraLoadoutEntry& Candidate) { return Candidate.Ability == Ability; });
}

const FVeyraLoadoutEntry* UVeyraAbilityLoadoutComponent::FindHandle(FGameplayAbilitySpecHandle Handle) const
{
	if (const FVeyraLoadoutEntry* Own = Entries.FindByPredicate([Handle](const FVeyraLoadoutEntry& Candidate) { return Candidate.Handle == Handle; }))
	{
		return Own;
	}
	if (const FVeyraSlotOverride* Override = Overrides.FindByPredicate([Handle](const FVeyraSlotOverride& Candidate) { return Candidate.Entry.Handle == Handle; }))
	{
		return &Override->Entry;
	}
	if (const FVeyraLoadoutEntry* Put = Stowed.FindByPredicate([Handle](const FVeyraLoadoutEntry& Candidate) { return Candidate.Handle == Handle; }))
	{
		return Put;
	}
	return Retired.FindByPredicate([Handle](const FVeyraLoadoutEntry& Candidate) { return Candidate.Handle == Handle; });
}

bool UVeyraAbilityLoadoutComponent::IsOverridden(EVeyraAbilitySlot Slot) const
{
	return Overrides.ContainsByPredicate([this, Slot](const FVeyraSlotOverride& Candidate) { return Candidate.Entry.Slot == Slot && IsShown(Candidate); });
}

FVeyraContentId UVeyraAbilityLoadoutComponent::CooldownIdOf(const FVeyraContentId& Ability) const
{
	const FVeyraSlotOverride* Override = Overrides.FindByPredicate([&Ability](const FVeyraSlotOverride& Candidate) { return Candidate.Entry.Ability == Ability; });
	if (Override && Override->bSharesCooldown)
	{
		// The own ability it belongs to, whichever the slot holds now (ADR-031 §3).
		if (Override->Over.IsValid())
		{
			return Override->Over;
		}
		const EVeyraAbilitySlot Slot = Override->Entry.Slot;
		if (const FVeyraLoadoutEntry* Own = Entries.FindByPredicate([Slot](const FVeyraLoadoutEntry& Candidate) { return Candidate.Slot == Slot; }))
		{
			return Own->Ability;
		}
	}
	return Ability;
}

bool UVeyraAbilityLoadoutComponent::Override(UAbilitySystemComponent& AbilitySystem, EVeyraAbilitySlot Slot, const FVeyraOverrideSpec& Spec)
{
	check(GetOwner() && GetOwner()->HasAuthority());
	const TSubclassOf<UVeyraGameplayAbility> Archetype = ArchetypeFor(Spec.Ability);
	if (!Archetype || !FMath::IsFinite(Spec.DurationSeconds) || Spec.DurationSeconds < 0.0)
	{
		UE_LOG(LogVeyraAbilities, Error, TEXT("Cannot override %s's slot with %s: the Abilities tuning defines no ability with that ID, or its time is not 0 or more."),
			*GetNameSafe(GetOwner()), *Spec.Ability.ToString());
		return false;
	}
	EndOverride(AbilitySystem, Slot);
	FVeyraSlotOverride& Override = Overrides.AddDefaulted_GetRef();
	Override.Entry.Slot = Slot;
	Override.Entry.Ability = Spec.Ability;
	Override.Entry.Handle = AbilitySystem.GiveAbility(FGameplayAbilitySpec(Archetype, DefaultAbilityLevel));
	Override.Use = Spec.Use;
	Override.Group = Spec.Group;
	Override.bCastOnExpiry = Spec.bCastOnExpiry;
	Override.bSharesCooldown = Spec.bSharesCooldown;
	Override.Over = Spec.Over;
	UWorld* World = GetWorld();
	if (World && Spec.DurationSeconds > 0.0)
	{
		Override.EndsAt = World->GetTimeSeconds() + Spec.DurationSeconds;
		FTimerHandle& Timer = OverrideTimers.FindOrAdd(Slot);
		World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &UVeyraAbilityLoadoutComponent::OnOverrideExpired, Slot),
			static_cast<float>(Spec.DurationSeconds), /*bLoop*/ false);
	}
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraAbilityLoadoutComponent, Overrides, this);
	UE_LOG(LogVeyraAbilities, Verbose, TEXT("%s's slot holds %s instead for %g s."), *GetNameSafe(GetOwner()), *Spec.Ability.ToString(), Spec.DurationSeconds);
	return Override.Entry.Handle.IsValid();
}

void UVeyraAbilityLoadoutComponent::EndOverride(UAbilitySystemComponent& AbilitySystem, EVeyraAbilitySlot Slot)
{
	const int32 Index = Overrides.IndexOfByPredicate([Slot](const FVeyraSlotOverride& Candidate) { return Candidate.Entry.Slot == Slot; });
	if (Index != INDEX_NONE)
	{
		RemoveOverrideAt(AbilitySystem, Index);
	}
}

void UVeyraAbilityLoadoutComponent::EndGroup(UAbilitySystemComponent& AbilitySystem, FName Group)
{
	for (int32 Index = Overrides.Num() - 1; Index >= 0; --Index)
	{
		if (!Group.IsNone() && Overrides[Index].Group == Group)
		{
			RemoveOverrideAt(AbilitySystem, Index);
		}
	}
}

void UVeyraAbilityLoadoutComponent::NoteCommitted(UAbilitySystemComponent& AbilitySystem, const FVeyraContentId& Ability)
{
	const int32 Index = Overrides.IndexOfByPredicate([&Ability](const FVeyraSlotOverride& Candidate) { return Candidate.Entry.Ability == Ability; });
	if (Index == INDEX_NONE || Overrides[Index].Use != EVeyraOverrideUse::Once)
	{
		return;
	}
	const FName Group = Overrides[Index].Group;
	RemoveOverrideAt(AbilitySystem, Index);
	EndGroup(AbilitySystem, Group);
}

void UVeyraAbilityLoadoutComponent::OnOverrideExpired(EVeyraAbilitySlot Slot)
{
	UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	const FVeyraSlotOverride* Override = Overrides.FindByPredicate([Slot](const FVeyraSlotOverride& Candidate) { return Candidate.Entry.Slot == Slot; });
	if (!AbilitySystem || !Override)
	{
		return;
	}
	// Copied: casting it may end it, or open another override in its place.
	const FVeyraContentId Ability = Override->Entry.Ability;
	const double EndsAt = Override->EndsAt;
	const bool bCastOnExpiry = Override->bCastOnExpiry;
	// Invalidated first: the handle is still active inside its own callback.
	if (FTimerHandle* Timer = OverrideTimers.Find(Slot))
	{
		Timer->Invalidate();
	}
	if (bCastOnExpiry)
	{
		// Its payoff is guaranteed; only its timing was the caster's (Roster Bible §1, Last Exit). It
		// goes where the caster faces.
		const AActor* Avatar = AbilitySystem->GetAvatarActor();
		FVeyraCastTarget Ahead;
		Ahead.bHasLocation = Avatar != nullptr;
		Ahead.Location = Avatar ? Avatar->GetActorLocation() + Avatar->GetActorForwardVector() : FVector::ZeroVector;
		const EVeyraCastRejection Rejection = VeyraAbilities::TryCast(*AbilitySystem, Slot, Ahead);
		UE_CLOG(Rejection != EVeyraCastRejection::None, LogVeyraAbilities, Verbose, TEXT("%s's %s could not cast itself as it expired (%s)."),
			*GetNameSafe(GetOwner()), *Ability.ToString(), *UEnum::GetValueAsString(Rejection));
	}
	// Unless its cast ended it already; a windup under way still finishes, its ability leaving after.
	const FVeyraSlotOverride* Still = Overrides.FindByPredicate([Slot](const FVeyraSlotOverride& Candidate) { return Candidate.Entry.Slot == Slot; });
	if (Still && Still->Entry.Ability == Ability && Still->EndsAt == EndsAt)
	{
		EndOverride(*AbilitySystem, Slot);
	}
}

void UVeyraAbilityLoadoutComponent::RemoveOverrideAt(UAbilitySystemComponent& AbilitySystem, int32 Index)
{
	const FVeyraSlotOverride Ended = Overrides[Index];
	Overrides.RemoveAt(Index);
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraAbilityLoadoutComponent, Overrides, this);
	if (FTimerHandle* Timer = OverrideTimers.Find(Ended.Entry.Slot))
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(*Timer);
		}
	}
	// A cast of it may still be running: its ability leaves once it ends, and can be named until then.
	Retired.RemoveAll([&AbilitySystem](const FVeyraLoadoutEntry& Old) { return AbilitySystem.FindAbilitySpecFromHandle(Old.Handle) == nullptr; });
	Retired.Add(Ended.Entry);
	AbilitySystem.SetRemoveAbilityOnEnd(Ended.Entry.Handle);
}
