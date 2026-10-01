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

	/** Server: the remaining cooldown of the ability Caster holds in Slot now shortens by Seconds (ADR-031 §10). */
	VEYRAABILITIES_API void ShortenCooldown(UAbilitySystemComponent& Caster, EVeyraAbilitySlot Slot, double Seconds);

	/**
	 * Ability's rank for Caster: its slot's rank from Progression, an override or stowed ability sharing
	 * its slot's (ADR-018 §1; ADR-031 §3); 1 in a slot that takes no ranks; 0 for one Caster does not hold.
	 */
	VEYRAABILITIES_API int32 RankOf(const UAbilitySystemComponent& Caster, const FVeyraContentId& Ability);

	/** What Ability costs Caster at its rank now, before any reduction (Combat Bible §27). */
	VEYRAABILITIES_API double ResourceCostOf(const UAbilitySystemComponent& Caster, const FVeyraContentId& Ability);

	/** Server: the follow-up OpenedBy's cast opens in Caster's slot ends, if it is open there or waits in another stance (ADR-032 §5). */
	VEYRAABILITIES_API void EndFollowUp(UAbilitySystemComponent& Caster, const FVeyraContentId& OpenedBy);
}
