// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "CoreMinimal.h"

class UAbilitySystemComponent;
class UWorld;
struct FVeyraCritTuning;

/** Whether one crit-capable action crits, and what its base damage is multiplied by if it does. */
struct FVeyraCritOutcome
{
	bool bCritical = false;

	/** The attack's Crit Damage; meaningful only when it crits. */
	double Multiplier = 1.0;
};

/** Combat's critical-strike rule (Combat Bible §5; ADR-022 §1), as one plain function. */
namespace VeyraCrit
{
	/**
	 * A basic attack with Chance and DamageBonus (the attacker's Crit Chance and Crit Damage Bonus)
	 * and a uniform Roll in [0, 1). It crits when Roll is below the chance, which counts only up to
	 * the cap. Its multiplier is the tuning's crit damage, plus DamageBonus, plus the overflow: each 1
	 * of chance above the cap adds OverflowDamagePerChance. Negative stats count as 0.
	 */
	VEYRACOMBAT_API FVeyraCritOutcome Resolve(double Chance, double DamageBonus, const FVeyraCritTuning& Tuning, double Roll);

	/** The channel every basic attack of a unit draws its crits from (ADR-022 §10): an identity, not tuning. */
	VEYRACOMBAT_API const FVeyraContentId& BasicAttackChannel();

	/**
	 * Whether one of Unit's crit-capable actions crits (ADR-022 §10). The action names its own
	 * Channel, its independent source of chance, and the Chance that governs it now: Unit's next
	 * outcome on Channel is drawn from a bag of the crit tuning's size, not rolled afresh, so a chance
	 * lands in its proportion over each bag without long streaks. Resolved as Resolve does, on the
	 * server; a chance of 0 draws nothing.
	 */
	VEYRACOMBAT_API FVeyraCritOutcome Check(const UWorld* World, const UAbilitySystemComponent& Unit, const FVeyraContentId& Channel, double Chance,
		double DamageBonus);
}
