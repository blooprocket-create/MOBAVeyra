// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"

struct FVeyraMovementTuning;
struct FVeyraSpeedSoftCap;

/** What a unit's effective Movement Speed is computed from, at one moment. */
struct FVeyraSpeedInputs
{
	/** The Move Speed stat after its §41 flat and percentage modifiers. */
	double MoveSpeed = 0.0;

	/** The Move Speed stat's base value, before any modifier. */
	double BaseMoveSpeed = 0.0;

	/** A bonus that applies only while its condition holds, as a fraction: 0.1 is 10% faster. */
	double ConditionalBonus = 0.0;

	/** The strongest active Slow, as the fraction of speed it removes; 0 when unslowed. */
	double StrongestSlow = 0.0;

	/** A Stun stops all movement. */
	bool bStunned = false;
};

/** Combat's Movement Speed rules (Combat Bible §23, §39), as plain functions. */
namespace VeyraMovementRules
{
	/** Speed after the soft caps: above each cap's start, only its retained fraction of the extra counts. */
	VEYRACOMBAT_API double ApplySoftCaps(double Speed, TConstArrayView<FVeyraSpeedSoftCap> SoftCaps);

	/**
	 * A unit's effective Movement Speed (ADR-009 §2): Move Speed, then the conditional bonus, then the
	 * strongest Slow, then the soft caps. Slowing never takes it below the slow floor, or below the
	 * unit's base speed where that is lower; a Stun makes it 0.
	 */
	VEYRACOMBAT_API double EffectiveSpeed(const FVeyraSpeedInputs& Inputs, const FVeyraMovementTuning& Tuning);
}
