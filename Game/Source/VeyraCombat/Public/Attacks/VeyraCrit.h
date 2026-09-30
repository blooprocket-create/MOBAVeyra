// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"

struct FVeyraCritTuning;

/** Whether one basic attack crits, and what its base damage is multiplied by if it does. */
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
}
