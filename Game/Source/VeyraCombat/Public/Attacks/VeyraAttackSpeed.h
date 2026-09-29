// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"

struct FVeyraAttackSpeedTuning;

/** How often a unit attacks at one moment, and what its overflow Attack Speed adds (Combat Bible §22). */
struct FVeyraAttackTiming
{
	/** Attacks per second, held between the minimum and the cap. */
	double AttacksPerSecond = 0.0;

	/** Seconds from the start of one attack to the start of the next. */
	double IntervalSeconds = 0.0;

	/** What overflow multiplies the basic attack's base damage by; 1 without overflow. */
	double OverflowDamageMultiplier = 1.0;
};

/** Combat's Attack Speed rules (Combat Bible §22, §39), as one plain function (ADR-009 §5). */
namespace VeyraAttackSpeed
{
	/**
	 * The timing of a unit whose uncapped Attack Speed is UncappedAttacksPerSecond. The interval is
	 * never shorter than MinimumIntervalSeconds, a kit's personal floor (0 for none). A temporary
	 * RaisedCap above the ordinary one lets it attack faster (0 for none). Overflow is always measured
	 * from the ordinary cap, the permanent reference (§22), so neither creates overflow of its own.
	 */
	VEYRACOMBAT_API FVeyraAttackTiming Resolve(double UncappedAttacksPerSecond, const FVeyraAttackSpeedTuning& Tuning, double MinimumIntervalSeconds,
		double RaisedCap = 0.0);
}
