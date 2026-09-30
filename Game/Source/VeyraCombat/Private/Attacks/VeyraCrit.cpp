// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attacks/VeyraCrit.h"

#include "Tuning/VeyraCombatTuning.h"

namespace VeyraCrit
{
FVeyraCritOutcome Resolve(double Chance, double DamageBonus, const FVeyraCritTuning& Tuning, double Roll)
{
	const double Listed = FMath::Max(0.0, Chance);
	const double Effective = FMath::Min(Listed, Tuning.ChanceCap);
	FVeyraCritOutcome Outcome;
	Outcome.bCritical = Roll < Effective;
	// Chance above the cap is not wasted: it becomes Crit Damage (§5, "Crit overflow").
	const double Overflow = FMath::Max(0.0, Listed - Tuning.ChanceCap);
	Outcome.Multiplier = Tuning.Damage + FMath::Max(0.0, DamageBonus) + Overflow * Tuning.OverflowDamagePerChance;
	return Outcome;
}
}
