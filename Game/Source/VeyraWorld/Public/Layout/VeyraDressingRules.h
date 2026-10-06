// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Math/Vector2D.h"

struct FVeyraWorldTuning;

/**
 * Where decoration may stand (ADR-040 §7): generated dressing must never change the battleground's competitive topology,
 * so it keeps off what play reads and walks.
 */
namespace VeyraDressing
{
	/**
	 * Whether a piece of dressing Radius across its footprint may stand at Point: clear of every lane's road and its wall
	 * clearance, every structure, both fountains, every camp's leash, every Flux Well, the Dense Fog (dressed by its own
	 * family) and the river's water.
	 */
	VEYRAWORLD_API bool Allows(const FVeyraWorldTuning& Tuning, const FVector2D& Point, double Radius);
}
