// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Life/VeyraCombatEventSubsystem.h"

class UAbilitySystemComponent;

/** Who is credited with a death (Combat Bible §18; ADR-011 §6). No world: the caller supplies the facts. */
namespace VeyraKillCredit
{
	/**
	 * The enemy Vanguard credited with a death. LethalSource is credited when it is an enemy Vanguard.
	 * Otherwise, as when a tower or a Fluxborn finishes the victim, the enemy Vanguard among
	 * Contributions that contributed most recently, no longer than WindowSeconds before Now, is; the
	 * lowest unique ID breaks a tie. Null for an Execution: nobody qualifies.
	 */
	VEYRACOMBAT_API UAbilitySystemComponent* Resolve(UAbilitySystemComponent* LethalSource, bool bLethalSourceIsEnemyVanguard,
		TConstArrayView<FVeyraContribution> Contributions, double Now, double WindowSeconds);
}
