// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Math/Vector2D.h"
#include "Misc/Optional.h"

/**
 * A lane as a bot walks it (ADR-013 §4): a path from its own base toward the enemy's, measured in
 * distance along it. Pure, so where a bot holds is tested without a world.
 */
namespace VeyraBotLane
{
	/** How far along Path the point nearest Location lies. */
	VEYRABOTS_API double DistanceAlong(TConstArrayView<FVector2D> Path, const FVector2D& Location);

	/** The point Distance along Path, clamped to its ends. */
	VEYRABOTS_API FVector2D PointAt(TConstArrayView<FVector2D> Path, double Distance);

	/**
	 * How far along its lane a bot holds (ADR-013 §4): FollowDistance behind its wave's front, or at
	 * its outermost standing structure when it has no wave; never inside the enemy structure's reach,
	 * EnemyReach being the distance along the lane where that reach begins, unless its wave holds the
	 * structure's attention. Never behind its own base.
	 */
	VEYRABOTS_API double HoldDistance(TOptional<double> WaveFront, double OwnStructure, TOptional<double> EnemyReach, bool bWaveHoldsTower,
		double FollowDistance);
}
