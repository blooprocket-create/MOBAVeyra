// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attacks/VeyraAttackSpeed.h"

#include "Tuning/VeyraCombatTuning.h"

namespace VeyraAttackSpeed
{
namespace
{
	// The overflow formula works in percentages (Combat Bible §22); this converts a fraction to one.
	constexpr double PercentPerWhole = 100.0;
}

FVeyraAttackTiming Resolve(double UncappedAttacksPerSecond, const FVeyraAttackSpeedTuning& Tuning, double MinimumIntervalSeconds)
{
	FVeyraAttackTiming Timing;
	const double Uncapped = FMath::IsFinite(UncappedAttacksPerSecond) ? UncappedAttacksPerSecond : 0.0;
	Timing.AttacksPerSecond = FMath::Clamp(Uncapped, Tuning.Minimum, Tuning.Cap);
	Timing.IntervalSeconds = FMath::Max(1.0 / Timing.AttacksPerSecond, MinimumIntervalSeconds);

	// Overflow is how far uncapped Attack Speed exceeds the cap, as a percentage of the cap.
	const double OverflowPercent = FMath::Max(0.0, Uncapped - Tuning.Cap) / Tuning.Cap * PercentPerWhole;
	const double BonusPercent = Tuning.OverflowDamageScalePercent * OverflowPercent / (Tuning.OverflowCurveConstant + OverflowPercent);
	Timing.OverflowDamageMultiplier = 1.0 + BonusPercent / PercentPerWhole;
	return Timing;
}
}
