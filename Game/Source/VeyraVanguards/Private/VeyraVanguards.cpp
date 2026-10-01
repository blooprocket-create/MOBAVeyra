// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraVanguards.h"

#include "AbilitySystemComponent.h"
#include "Attacks/VeyraBasicAttackComponent.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Passives/VeyraAttackStridePassive.h"
#include "Passives/VeyraBreachPassive.h"
#include "Passives/VeyraChargerPassive.h"
#include "Passives/VeyraCampRewardPassive.h"
#include "Passives/VeyraCadencePassive.h"
#include "Passives/VeyraDeepFoundationPassive.h"
#include "Passives/VeyraDisciplinesPassive.h"
#include "Passives/VeyraGatheringLightPassive.h"
#include "Passives/VeyraHauntPassive.h"
#include "Passives/VeyraKitStatusesPassive.h"
#include "Passives/VeyraMomentumPassive.h"
#include "Passives/VeyraMovingTargetPassive.h"
#include "Passives/VeyraAccordPassive.h"
#include "Passives/VeyraQuarryPassive.h"
#include "Passives/VeyraReclaimPassive.h"
#include "Passives/VeyraStressTemperPassive.h"
#include "Passives/VeyraSlipstreamPassive.h"
#include "Passives/VeyraUnreturnedPassive.h"
#include "Passives/VeyraWildDominionPassive.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Progression/VeyraProgressionTuningSubsystem.h"
#include "Shared/VeyraHitChainPassive.h"
#include "Shared/VeyraMarkProcPassive.h"
#include "Slots/VeyraAbilitySlot.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "VeyraVanguardsLog.h"

namespace VeyraVanguards
{
TSubclassOf<UVeyraPassive> PassiveClassFor(const FVeyraContentId& PassiveId)
{
	if (UVeyraVanguardsTuningSubsystem::FindDeepFoundation(PassiveId))
	{
		return UVeyraDeepFoundationPassive::StaticClass();
	}
	if (UVeyraVanguardsTuningSubsystem::FindHitChain(PassiveId))
	{
		return UVeyraHitChainPassive::StaticClass();
	}
	if (UVeyraVanguardsTuningSubsystem::FindGatheringLight(PassiveId))
	{
		return UVeyraGatheringLightPassive::StaticClass();
	}
	if (UVeyraVanguardsTuningSubsystem::FindBreach(PassiveId))
	{
		return UVeyraBreachPassive::StaticClass();
	}
	if (UVeyraVanguardsTuningSubsystem::FindMovingTarget(PassiveId))
	{
		return UVeyraMovingTargetPassive::StaticClass();
	}
	if (UVeyraVanguardsTuningSubsystem::FindCadence(PassiveId))
	{
		return UVeyraCadencePassive::StaticClass();
	}
	if (UVeyraVanguardsTuningSubsystem::FindMarkProc(PassiveId))
	{
		return UVeyraMarkProcPassive::StaticClass();
	}
	if (UVeyraVanguardsTuningSubsystem::FindHaunt(PassiveId))
	{
		return UVeyraHauntPassive::StaticClass();
	}
	if (UVeyraVanguardsTuningSubsystem::FindCampReward(PassiveId))
	{
		return UVeyraCampRewardPassive::StaticClass();
	}
	if (UVeyraVanguardsTuningSubsystem::FindMomentum(PassiveId))
	{
		return UVeyraMomentumPassive::StaticClass();
	}
	if (UVeyraVanguardsTuningSubsystem::FindWildDominion(PassiveId))
	{
		return UVeyraWildDominionPassive::StaticClass();
	}
	if (UVeyraVanguardsTuningSubsystem::FindKitStatuses(PassiveId))
	{
		return UVeyraKitStatusesPassive::StaticClass();
	}
	if (UVeyraVanguardsTuningSubsystem::FindAttackStride(PassiveId))
	{
		return UVeyraAttackStridePassive::StaticClass();
	}
	if (UVeyraVanguardsTuningSubsystem::FindSlipstream(PassiveId))
	{
		return UVeyraSlipstreamPassive::StaticClass();
	}
	if (UVeyraVanguardsTuningSubsystem::FindReclaim(PassiveId))
	{
		return UVeyraReclaimPassive::StaticClass();
	}
	if (UVeyraVanguardsTuningSubsystem::FindQuarry(PassiveId))
	{
		return UVeyraQuarryPassive::StaticClass();
	}
	if (UVeyraVanguardsTuningSubsystem::FindDisciplines(PassiveId))
	{
		return UVeyraDisciplinesPassive::StaticClass();
	}
	if (UVeyraVanguardsTuningSubsystem::FindUnreturned(PassiveId))
	{
		return UVeyraUnreturnedPassive::StaticClass();
	}
	if (UVeyraVanguardsTuningSubsystem::FindStressTemper(PassiveId))
	{
		return UVeyraStressTemperPassive::StaticClass();
	}
	if (UVeyraVanguardsTuningSubsystem::FindCharger(PassiveId))
	{
		return UVeyraChargerPassive::StaticClass();
	}
	if (UVeyraVanguardsTuningSubsystem::FindAccord(PassiveId))
	{
		return UVeyraAccordPassive::StaticClass();
	}
	return nullptr;
}

FVeyraPreparedVanguard PrepareCombatant(UAbilitySystemComponent& AbilitySystem, const FVeyraContentId& Vanguard)
{
	const FVeyraVanguardDefinition* Definition = UVeyraVanguardsTuningSubsystem::FindVanguard(Vanguard);
	AActor* Participant = AbilitySystem.GetOwner();
	UVeyraAbilityLoadoutComponent* Loadout = Participant ? Participant->FindComponentByClass<UVeyraAbilityLoadoutComponent>() : nullptr;
	UVeyraProgressionComponent* Progression = Participant ? Participant->FindComponentByClass<UVeyraProgressionComponent>() : nullptr;
	UVeyraBasicAttackComponent* Attacks = Participant ? Participant->FindComponentByClass<UVeyraBasicAttackComponent>() : nullptr;
	if (!Definition || !Loadout || !Progression || !Attacks)
	{
		UE_LOG(LogVeyraVanguards, Error, TEXT("Cannot make %s into Vanguard %s: %s."), *GetNameSafe(Participant), *Vanguard.ToString(),
			Definition ? TEXT("it lacks a loadout, progression or basic attack component") : TEXT("Vanguards.json does not define it"));
		return FVeyraPreparedVanguard();
	}

	if (!VeyraCombat::InitializeStats(AbilitySystem, Definition->BaseStats) || !Attacks->SetProfile(Definition->BasicAttack))
	{
		return FVeyraPreparedVanguard();
	}
	// Charge starts empty and is kept (ADR-033 §1).
	if (Definition->Resource == EVeyraResourceFamily::Charge)
	{
		VeyraCombat::KeepResource(AbilitySystem);
	}
	const TPair<EVeyraAbilitySlot, const TArray<FVeyraContentId>*> Kit[] = { { EVeyraAbilitySlot::Q, &Definition->Abilities.Q },
		{ EVeyraAbilitySlot::W, &Definition->Abilities.W }, { EVeyraAbilitySlot::E, &Definition->Abilities.E }, { EVeyraAbilitySlot::R, &Definition->Abilities.R } };
	for (const TPair<EVeyraAbilitySlot, const TArray<FVeyraContentId>*>& Slot : Kit)
	{
		for (const FVeyraContentId& Ability : *Slot.Value)
		{
			if (!Loadout->Grant(AbilitySystem, Slot.Key, Ability))
			{
				return FVeyraPreparedVanguard();
			}
		}
	}
	// Level 1, with that level's skill point; the player chooses the first rank. A rank shape may
	// learn R from the start (ADR-031 §2).
	const FVeyraRankShape Shape = VeyraVanguardRules::RankShapeOf(*Definition, UVeyraProgressionTuningSubsystem::Get());
	Progression->Initialize(Definition->Growth, Definition->BaseStats.AttackSpeed, &Shape);

	FVeyraPreparedVanguard Prepared;
	Prepared.bPrepared = true;
	for (const FVeyraContentId& PassiveId : Definition->Passive)
	{
		const TSubclassOf<UVeyraPassive> PassiveClass = PassiveClassFor(PassiveId);
		if (!PassiveClass)
		{
			UE_LOG(LogVeyraVanguards, Error, TEXT("Vanguard %s names passive %s, which no passive class runs."), *Vanguard.ToString(), *PassiveId.ToString());
			return FVeyraPreparedVanguard();
		}
		Prepared.Passive = NewObject<UVeyraPassive>(Participant, PassiveClass);
		Prepared.Passive->Start(AbilitySystem, PassiveId);
	}
	UE_LOG(LogVeyraVanguards, Log, TEXT("%s is Vanguard %s."), *GetNameSafe(Participant), *Vanguard.ToString());
	return Prepared;
}
}
