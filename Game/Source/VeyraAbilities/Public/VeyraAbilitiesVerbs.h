// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "VeyraAbilityTypes.h"

class UAbilitySystemComponent;

/**
 * Abilities' verbs (ARCHITECTURE.md §1.10): the one way gameplay code casts an ability. Server only.
 */
namespace VeyraAbilities
{
	/**
	 * Casts the ability in Slot at Target: validates it with the ability's own rules and, if they
	 * allow it, activates it on the server, which commits and resolves it. Returns why it was
	 * refused, or None. Whether the match allows casting at all (phase, pause) is the caller's.
	 */
	VEYRAABILITIES_API EVeyraCastRejection TryCast(UAbilitySystemComponent& Caster, EVeyraAbilitySlot Slot, const FVeyraCastTarget& Target);

	/**
	 * Server: shortens the running cooldown of Ability, wherever Caster's loadout keeps it, by Fraction of
	 * what remains (ADR-030 §3). An ability that is ready is left alone.
	 */
	VEYRAABILITIES_API void RefundCooldown(UAbilitySystemComponent& Caster, const FVeyraContentId& Ability, double Fraction);

	/** Server: the same for the abilities Caster holds in Slots now. */
	VEYRAABILITIES_API void RefundCooldowns(UAbilitySystemComponent& Caster, TConstArrayView<EVeyraAbilitySlot> Slots, double Fraction);
}
