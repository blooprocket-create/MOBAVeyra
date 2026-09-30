// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Everything a unit's equipment adds to its stats, together (ADR-012 §6): flat amounts added to the
 * base (Combat Bible §41, step 2) and percentage bonuses on top (step 3). Items sums its delivered
 * items into one and VeyraCombat::SetEquipmentStats applies it. Every value is finite and at least 0.
 */
struct FVeyraEquipmentStats
{
	double MaxHealth = 0.0;
	double HealthRegen = 0.0;
	double PhysicalPower = 0.0;
	double MagicPower = 0.0;

	/** Attacks per second: an item's bonus Attack Speed, already turned into a flat amount from the unit's base. */
	double AttackSpeed = 0.0;

	double AbilityHaste = 0.0;
	double MoveSpeed = 0.0;
	double MagicPenetrationFlat = 0.0;

	/** Magic Power's percentage bonus, as a fraction: 0.3 is +30% (§41, step 3). */
	double MagicPowerFraction = 0.0;

	/** Crit Chance and Crit Damage Bonus, as fractions (Combat Bible §5; ADR-022 §2). */
	double CritChance = 0.0;
	double CritDamageBonus = 0.0;
};
