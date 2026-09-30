// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attacks/VeyraCrit.h"

#include "Attacks/VeyraCombatRollSubsystem.h"
#include "Tuning/VeyraCombatTuning.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"

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

const FVeyraContentId& BasicAttackChannel()
{
	static const FVeyraContentId Channel = FVeyraContentId::FromText(TEXT("basic_attack")).GetValue();
	return Channel;
}

FVeyraCritOutcome Check(const UWorld* World, const UAbilitySystemComponent& Unit, const FVeyraContentId& Channel, double Chance, double DamageBonus)
{
	const FVeyraCombatTuning& Tuning = UVeyraCombatTuningSubsystem::Get();
	// Nothing to draw for a unit that cannot crit, such as a Fluxborn: no bag is kept for it.
	constexpr double NeverCrits = 1.0;
	const double Roll = Chance > 0.0 ? UVeyraCombatRollSubsystem::Draw(World, Unit, Channel, Tuning.CritBag.Draws) : NeverCrits;
	return Resolve(Chance, DamageBonus, Tuning.Crit, Roll);
}
}
