// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/** Haste's arithmetic (Combat Bible §21, §39). */
namespace VeyraHaste
{
	/**
	 * The share of a cooldown that remains with Haste: 100 / (100 + Haste), with Haste floored at 0
	 * (§39: a longer cooldown is a separate modifier, never negative Haste).
	 */
	VEYRACOMBAT_API double CooldownMultiplier(double Haste);
}
