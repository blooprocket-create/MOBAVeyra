// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Records/VeyraCombatRecords.h"

#include "AbilitySystemComponent.h"
#include "Attribution/VeyraAttributionComponent.h"
#include "CombatState/VeyraCombatStateComponent.h"
#include "Engine/World.h"
#include "Targeting/VeyraTargeting.h"
#include "Units/VeyraUnit.h"

namespace VeyraCombatRecords
{
void NoteHostileAction(UAbilitySystemComponent* Source, UAbilitySystemComponent& Target)
{
	AActor* SourceUnit = Source ? Source->GetOwner() : nullptr;
	AActor* TargetUnit = Target.GetOwner();
	const bool bEnemyVanguards = SourceUnit && TargetUnit && SourceUnit != TargetUnit && VeyraUnits::IsVanguard(SourceUnit)
		&& VeyraUnits::IsVanguard(TargetUnit) && VeyraTargeting::AreHostile(SourceUnit, TargetUnit);
	if (!bEnemyVanguards || !TargetUnit->HasAuthority())
	{
		return;
	}

	for (AActor* Unit : { SourceUnit, TargetUnit })
	{
		if (UVeyraCombatStateComponent* CombatState = Unit->FindComponentByClass<UVeyraCombatStateComponent>())
		{
			CombatState->NoteCombat();
		}
	}
	if (UVeyraAttributionComponent* Attribution = TargetUnit->FindComponentByClass<UVeyraAttributionComponent>())
	{
		Attribution->NoteContribution(*Source, TargetUnit->GetWorld()->GetTimeSeconds());
	}
}
}
