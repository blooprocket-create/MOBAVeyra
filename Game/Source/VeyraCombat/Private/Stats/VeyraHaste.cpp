// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Stats/VeyraHaste.h"

namespace VeyraHaste
{
namespace
{
	// Part of Haste's definition (Combat Bible §21), not tuning: 100 Haste halves a cooldown.
	constexpr double HasteScale = 100.0;
}

double CooldownMultiplier(double Haste)
{
	return HasteScale / (HasteScale + FMath::Max(0.0, Haste));
}
}
