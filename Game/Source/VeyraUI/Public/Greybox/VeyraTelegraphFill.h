// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Greybox/VeyraGreyboxSubsystem.h"
#include "Math/Vector.h"
#include "Math/Vector2D.h"

/** Where a telegraph's shaded fill lies (ADR-068 §4): a flat quad, and the shape the fill material draws in it. */
struct FVeyraTelegraphFill
{
	/** The quad's centre on the ground, its yaw in degrees (its +U along the shape's direction), and its half-size in units. */
	FVector Centre = FVector::ZeroVector;
	double Yaw = 0.0;
	FVector2D HalfSize = FVector2D::ZeroVector;

	/** The shape the material draws: 0 a circle, 1 a sector facing +U, 2 a rectangle from its origin end at U = 0. */
	int32 Shape = 0;

	/** A sector's half-arc, in radians; a whole turn's half for the rest. */
	double HalfArc = UE_DOUBLE_PI;
};

/** A telegraph's shaded fill, as plain functions of what the client knows, so tests check them (ADR-068 §4). */
namespace VeyraTelegraphFill
{
	/** Where Placed's fill lies, its origin on the ground at Origin. */
	VEYRAUI_API FVeyraTelegraphFill Of(const FVeyraPlacedShape& Placed, const FVector& Origin);

	/**
	 * Whether a telegraph of Source is filled: what threatens or aims (a windup, a channel, a delayed or lingering area, the
	 * player's indicator, a projectile's lane); never a ring that only marks, as the selection's or the attack range's.
	 */
	VEYRAUI_API bool IsFilled(EVeyraTelegraphSource Source);

	/**
	 * How far a telegraph of Source has landed, from 0 to 1: over the last LandingSeconds before it lands (RemainingSeconds
	 * to go) for one that lands, as a windup, a channel's tick, a delayed area or a lingering area's end; 0 for the rest.
	 */
	VEYRAUI_API double LandingOf(EVeyraTelegraphSource Source, double RemainingSeconds, double LandingSeconds);
}
