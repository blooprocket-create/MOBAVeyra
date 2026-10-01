// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Quests/VeyraQuestRules.h"

#include "AbilitySystemComponent.h"
#include "Inventory/VeyraInventoryRules.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraItemsTuning.h"
#include "Units/VeyraUnit.h"

namespace VeyraQuests
{
AActor* LaneFluxbornLastHitter(const FVeyraDeathEvent& Death)
{
	const UAbilitySystemComponent* Killer = Death.Killer.Get();
	const UAbilitySystemComponent* Victim = Death.Victim.Get();
	AActor* Participant = Killer ? Killer->GetOwner() : nullptr;
	const AActor* Unit = Victim ? Victim->GetAvatarActor() : nullptr;
	const bool bLaneFluxborn = VeyraUnits::KindOf(Unit) == EVeyraUnitKind::Fluxborn;
	return bLaneFluxborn && VeyraUnits::IsVanguard(Participant) && VeyraTargeting::AreHostile(Participant, Unit) ? Participant : nullptr;
}

TArray<FVeyraContentId> Advance(const FVeyraItemsTuning& Tuning, TArray<FVeyraInventorySlot>& Slots, EVeyraQuestObjective Objective)
{
	TArray<FVeyraContentId> Evolved;
	for (FVeyraInventorySlot& Slot : Slots)
	{
		const FVeyraQuestTuning* Quest = Slot.IsEmpty() ? nullptr : Tuning.Quests.Find(Slot.Item);
		if (!Quest || Quest->Objective != Objective || ++Slot.QuestProgress < Quest->Threshold)
		{
			continue;
		}
		Slot.Item = Quest->EvolvesInto;
		Slot.QuestProgress = 0;
		Slot.bBenefited = true;
		Evolved.Add(Quest->EvolvesInto);
	}
	return Evolved;
}
}
