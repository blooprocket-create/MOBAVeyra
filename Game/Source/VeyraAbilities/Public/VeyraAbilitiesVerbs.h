// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Templates/SubclassOf.h"
#include "VeyraAbilityTypes.h"

class UAbilitySystemComponent;
class UVeyraGameplayAbility;

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
	 * Server: of the abilities Caster holds in Slots now, the one whose cooldown ends soonest shortens by
	 * Seconds (ADR-033 §6); none that is ready.
	 */
	VEYRAABILITIES_API void ShortenSoonestCooldown(UAbilitySystemComponent& Caster, TConstArrayView<EVeyraAbilitySlot> Slots, double Seconds);

	/**
	 * Ability's rank for Caster: its slot's rank from Progression, an override or stowed ability sharing
	 * its slot's (ADR-018 §1; ADR-031 §3); 1 in a slot that takes no ranks; 0 for one Caster does not hold.
	 */
	VEYRAABILITIES_API int32 RankOf(const UAbilitySystemComponent& Caster, const FVeyraContentId& Ability);

	/** What Ability costs Caster at its rank now, before any reduction (Combat Bible §27). */
	VEYRAABILITIES_API double ResourceCostOf(const UAbilitySystemComponent& Caster, const FVeyraContentId& Ability);

	/** The archetype class that runs Ability, from the map the Abilities tuning defines it in (ADR-008 §3); null for none. */
	VEYRAABILITIES_API TSubclassOf<UVeyraGameplayAbility> ArchetypeOf(const FVeyraContentId& Ability);

	/**
	 * Server and the owner's client: whether Caster holds enough of its resource to cast Ability now, by the test the cast
	 * validator refuses InsufficientResource by (ADR-066 §1). True for an ability its archetype does not define, or that
	 * Caster has not learned.
	 */
	VEYRAABILITIES_API bool CanAffordCast(const UAbilitySystemComponent& Caster, const FVeyraContentId& Ability);

	/** What casting Ability would cost Caster now, as its Commit charges it (ADR-033 §3); 0 before it is learned. */
	VEYRAABILITIES_API double CastCostOf(const UAbilitySystemComponent& Caster, const FVeyraContentId& Ability);

	/** Server: the follow-up OpenedBy's cast opens in Caster's slot ends, if it is open there or waits in another stance (ADR-032 §5). */
	VEYRAABILITIES_API void EndFollowUp(UAbilitySystemComponent& Caster, const FVeyraContentId& OpenedBy);
}
