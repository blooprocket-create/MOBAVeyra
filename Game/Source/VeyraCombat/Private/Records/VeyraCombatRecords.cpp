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
	const bool bVanguardAgainstEnemy = SourceUnit && TargetUnit && SourceUnit != TargetUnit && VeyraUnits::IsVanguard(SourceUnit)
		&& VeyraTargeting::AreHostile(SourceUnit, TargetUnit);
	if (!bVanguardAgainstEnemy || !TargetUnit->HasAuthority())
	{
		return;
	}

	// Combat State is between Vanguards (Combat Bible §28).
	if (VeyraUnits::IsVanguard(TargetUnit))
	{
		for (AActor* Unit : { SourceUnit, TargetUnit })
		{
			if (UVeyraCombatStateComponent* CombatState = Unit->FindComponentByClass<UVeyraCombatStateComponent>())
			{
				CombatState->NoteCombat();
			}
		}
	}
	// Any unit that keeps attribution records the Vanguard: for assists on a Vanguard (§18), and for
	// Economy's participation and structure rewards on anything else (ADR-011 §6).
	if (UVeyraAttributionComponent* Attribution = TargetUnit->FindComponentByClass<UVeyraAttributionComponent>())
	{
		Attribution->NoteContribution(*Source, TargetUnit->GetWorld()->GetTimeSeconds());
	}
}
}
