// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Hud/VeyraHudModel.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "Attributes/VeyraResourceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Progression/VeyraProgressionRules.h"
#include "Progression/VeyraProgressionTuningSubsystem.h"
#include "Statuses/VeyraStatusComponent.h"
#include "VeyraPlayerState.h"

namespace
{
	/** A component beside Unit's Ability System Component: on a Vanguard, its participant's PlayerState. */
	template <typename ComponentType>
	const ComponentType* FindBesideHudAbilitySystem(const AActor& Unit)
	{
		const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit);
		const AActor* Owner = AbilitySystem ? AbilitySystem->GetOwner() : nullptr;
		return Owner ? Owner->FindComponentByClass<ComponentType>() : nullptr;
	}
}

TOptional<FVeyraHudVitals> VeyraHud::VitalsOf(const AActor& Unit)
{
	const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit);
	if (!AbilitySystem || !AbilitySystem->HasAttributeSetForAttribute(UVeyraVitalsSet::GetHealthAttribute()))
	{
		return {};
	}
	FVeyraHudVitals Vitals;
	Vitals.Health = AbilitySystem->GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute());
	Vitals.MaxHealth = AbilitySystem->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute());
	if (AbilitySystem->HasAttributeSetForAttribute(UVeyraResourceSet::GetResourceAttribute()))
	{
		Vitals.Resource = AbilitySystem->GetNumericAttribute(UVeyraResourceSet::GetResourceAttribute());
		Vitals.MaxResource = AbilitySystem->GetNumericAttribute(UVeyraResourceSet::GetMaxResourceAttribute());
	}
	if (const UVeyraDamageAbsorptionComponent* Absorption = FindBesideHudAbilitySystem<UVeyraDamageAbsorptionComponent>(Unit))
	{
		for (const FVeyraShieldEntry& Shield : Absorption->GetLedger().Shields)
		{
			Vitals.Shield += Shield.Remaining;
		}
	}
	return Vitals;
}

TArray<FVeyraHudStatus> VeyraHud::StatusesOf(const AActor& Unit, double ServerNow)
{
	TArray<FVeyraHudStatus> Statuses;
	if (const UVeyraStatusComponent* Ledger = FindBesideHudAbilitySystem<UVeyraStatusComponent>(Unit))
	{
		for (const FVeyraStatusEntry& Entry : Ledger->GetLedger().Entries)
		{
			Statuses.Add(FVeyraHudStatus{ Entry.Id, Entry.Kind, FMath::Max(0.0, Entry.EndsAt - ServerNow) });
		}
	}
	return Statuses;
}

FVeyraHudPlayer VeyraHud::DescribePlayer(const AVeyraPlayerState& Participant, double ServerNow)
{
	FVeyraHudPlayer Player;
	Player.Vanguard = Participant.GetVanguardId();
	if (const APawn* Vanguard = Participant.GetPawn())
	{
		Player.Vitals = VitalsOf(*Vanguard).Get(FVeyraHudVitals());
	}

	const FVeyraProgressionTuning& Tuning = UVeyraProgressionTuningSubsystem::Get();
	const UVeyraProgressionComponent* Progression = Participant.FindComponentByClass<UVeyraProgressionComponent>();
	if (Progression && Progression->IsInitialized())
	{
		Player.Level = Progression->GetLevel();
		Player.Experience = Progression->GetExperience();
		Player.ExperienceToNextLevel = VeyraProgression::ExperienceToNextLevel(Player.Level, Tuning);
		Player.UnspentSkillPoints = Progression->GetUnspentSkillPoints();
	}

	const UVeyraAbilityLoadoutComponent* Loadout = Participant.FindComponentByClass<UVeyraAbilityLoadoutComponent>();
	const UVeyraCooldownComponent* Cooldowns = Participant.FindComponentByClass<UVeyraCooldownComponent>();
	for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::All)
	{
		FVeyraHudSlot& Shown = Player.Slots.AddDefaulted_GetRef();
		Shown.Slot = Slot;
		Shown.MaxRank = VeyraProgression::MaxRank(Slot, Tuning);
		if (const FVeyraLoadoutEntry* Entry = Loadout ? Loadout->FindSlot(Slot) : nullptr)
		{
			Shown.Ability = Entry->Ability;
			Shown.CooldownSeconds = Cooldowns ? Cooldowns->GetRemainingSeconds(Entry->Ability, ServerNow) : 0.0;
		}
		if (Progression && Progression->IsInitialized())
		{
			Shown.Rank = Progression->GetRank(Slot);
			Shown.bCanRankUp = VeyraProgression::CheckRankUp(Slot, Shown.Rank, Player.Level, Player.UnspentSkillPoints, Tuning) == EVeyraRankRefusal::None;
		}
	}
	return Player;
}
