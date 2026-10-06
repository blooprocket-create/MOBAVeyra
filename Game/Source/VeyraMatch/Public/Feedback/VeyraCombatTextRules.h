// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Feedback/VeyraCombatTextTypes.h"
#include "Gold/VeyraGoldComponent.h"
#include "Misc/Optional.h"

class UAbilitySystemComponent;
struct FVeyraDamageDealtEvent;
struct FVeyraHealthRestored;
struct FVeyraShieldGranted;

/**
 * Which combat text numbers each of Combat's outcomes gives a player (ADR-052 §1). Player is a participant's Ability
 * System Component: the player's own numbers are what it, or a unit it owns, dealt or gave, and what its Vanguard
 * received. Whether the player's side sees the unit is the caller's question.
 */
namespace VeyraCombatTextRouting
{
	/** Damage the player dealt, one number per type at the target; else damage its Vanguard received. Empty for neither. */
	VEYRAMATCH_API TArray<FVeyraCombatTextLine> ForDamage(const FVeyraDamageDealtEvent& Event, const UAbilitySystemComponent& Player);

	/** Healing a unit gave, when the player gave or received it; nothing for regeneration, the fountain or a Well. */
	VEYRAMATCH_API TOptional<FVeyraCombatTextLine> ForHealing(const FVeyraHealthRestored& Event, const UAbilitySystemComponent& Player);

	/** A shield the player granted or received. */
	VEYRAMATCH_API TOptional<FVeyraCombatTextLine> ForShield(const FVeyraShieldGranted& Event, const UAbilitySystemComponent& Player);

	/**
	 * Gold the player earned (ADR-065 §4): fixed where the unit whose fall earned it fell, or else over Own, the player's
	 * Vanguard. Nothing for starting Gold, passive income, a sale, an undo or a developer's grant, nor without anywhere
	 * to show.
	 */
	VEYRAMATCH_API TOptional<FVeyraCombatTextLine> ForGold(double Amount, EVeyraGoldReason Reason, const FVeyraGoldSource& From, AActor* Own);
}
