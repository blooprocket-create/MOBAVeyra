// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Hud/VeyraHudModel.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "Attacks/VeyraBasicAttackComponent.h"
#include "Attributes/VeyraResourceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "Gold/VeyraGoldComponent.h"
#include "Ledger/VeyraFluxLedger.h"
#include "Life/VeyraLifeComponent.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Progression/VeyraProgressionRules.h"
#include "Progression/VeyraProgressionTuningSubsystem.h"
#include "State/VeyraTeamFluxState.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Structures/VeyraStructure.h"
#include "Tuning/VeyraFluxTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
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

TOptional<FVeyraHudStructure> VeyraHud::StructureOf(const AActor& Unit, double ServerNow)
{
	const AVeyraStructure* Structure = Cast<AVeyraStructure>(&Unit);
	if (!Structure)
	{
		return {};
	}
	FVeyraHudStructure Shown;
	Shown.Kind = Structure->GetStructureKind();
	Shown.bInvulnerable = Structure->IsInvulnerable();
	if (Structure->GetRebuildsAt() > 0.0)
	{
		Shown.RebuildSeconds = FMath::Max(0.0, Structure->GetRebuildsAt() - ServerNow);
	}
	return Shown;
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
		// Rounded down for display only: it never shows a level's XP before it is earned.
		Player.Experience = FMath::FloorToInt32(Progression->GetExperience());
		Player.ExperienceToNextLevel = VeyraProgression::ExperienceToNextLevel(Player.Level, Tuning);
		Player.UnspentSkillPoints = Progression->GetUnspentSkillPoints();
	}
	if (const UVeyraGoldComponent* Gold = Participant.FindComponentByClass<UVeyraGoldComponent>())
	{
		Player.Gold = FMath::FloorToInt32(Gold->GetGold());
	}
	if (const UVeyraLifeComponent* Life = Participant.FindComponentByClass<UVeyraLifeComponent>(); Life && !Life->IsAlive())
	{
		Player.bDead = true;
		Player.RespawnSeconds = FMath::Max(0.0, Participant.GetRespawnAt() - ServerNow);
	}

	if (const FVeyraVanguardDefinition* Definition = UVeyraVanguardsTuningSubsystem::FindVanguard(Player.Vanguard); Definition && !Definition->Passive.IsEmpty())
	{
		Player.Passive = Definition->Passive[0];
	}

	const UVeyraAbilityLoadoutComponent* Loadout = Participant.FindComponentByClass<UVeyraAbilityLoadoutComponent>();
	const UVeyraCooldownComponent* Cooldowns = Participant.FindComponentByClass<UVeyraCooldownComponent>();
	const UVeyraBasicAttackComponent* Attacks = Participant.FindComponentByClass<UVeyraBasicAttackComponent>();
	for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::All)
	{
		FVeyraHudSlot& Shown = Player.Slots.AddDefaulted_GetRef();
		Shown.Slot = Slot;
		Shown.MaxRank = VeyraProgression::MaxRank(Slot, Tuning);
		if (const FVeyraLoadoutEntry* Entry = Loadout ? Loadout->FindSlot(Slot) : nullptr)
		{
			Shown.Ability = Entry->Ability;
			Shown.CooldownSeconds = Cooldowns ? Cooldowns->GetRemainingSeconds(Entry->Ability, ServerNow) : 0.0;
			if (Attacks && Attacks->GetEmpowermentView().Ability == Entry->Ability)
			{
				Shown.EmpoweredSeconds = FMath::Max(0.0, Attacks->GetEmpowermentView().ExpiresAt - ServerNow);
			}
		}
		if (Progression && Progression->IsInitialized())
		{
			Shown.Rank = Progression->GetRank(Slot);
			Shown.bCanRankUp = VeyraProgression::CheckRankUp(Slot, Shown.Rank, Player.Level, Player.UnspentSkillPoints, Tuning) == EVeyraRankRefusal::None;
		}
	}
	return Player;
}

TArray<FVeyraHudTeamFlux> VeyraHud::DescribeTeamFlux(const UWorld* World, double ServerNow)
{
	TArray<FVeyraHudTeamFlux> Teams;
	const AVeyraTeamFluxState* State = AVeyraTeamFluxState::Find(World);
	if (!State)
	{
		return Teams;
	}
	for (const FVeyraTeamFluxView& View : State->GetTeams())
	{
		FVeyraHudTeamFlux& Shown = Teams.AddDefaulted_GetRef();
		Shown.Team = View.Team;
		Shown.Active = View.ActiveAt(ServerNow);
		Shown.Permanent = View.Permanent;
		// Flux's own rule says what the Flux gives; the HUD only shows it.
		Shown.FluxbornBonus = VeyraFlux::StrengthFor(Shown.Active, UVeyraFluxTuningSubsystem::Get().FluxbornScaling).HealthMultiplier - 1.0;
		for (const FVeyraTemporaryFluxView& Grant : View.Temporary)
		{
			if (Grant.ExpiresAt > ServerNow)
			{
				Shown.TemporarySeconds.Add(Grant.ExpiresAt - ServerNow);
			}
		}
		Shown.TemporarySeconds.Sort();
	}
	return Teams;
}
