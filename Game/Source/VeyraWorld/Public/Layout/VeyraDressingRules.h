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
	 * Whether a piece of dressing Radius across its footprint may stand at Point: clear of every lane's road and shoulders,
	 * every structure and Flux Well by a shoulder's margin, both fountains, every camp's leash, the Dense Fog (dressed by
	 * its own family) and the river's water.
	 */
	VEYRAWORLD_API bool Allows(const FVeyraWorldTuning& Tuning, const FVector2D& Point, double Radius);

	/**
	 * Whether a lane's edging, Radius across, may stand at Point along the edge of the lane at LaneIndex: flush with the
	 * road, it may stand on that lane's shoulder, but clear of the water, every structure and Flux Well by a shoulder's
	 * margin, both fountains, both keeps' floors and every other lane's road, so it never edges what play reads.
	 */
	VEYRAWORLD_API bool AllowsEdging(const FVeyraWorldTuning& Tuning, const FVector2D& Point, double Radius, int32 LaneIndex);
}
