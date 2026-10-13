// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Greybox/VeyraGreyboxSubsystem.h"
#include "Math/Vector.h"
#include "Math/Vector2D.h"

struct FVeyraAbilityEffects;
enum class EVeyraTelegraphShow : uint8;

/**
 * How an area lies over the ground (ADR-075 §3): the plane that best fits the ground under its footprint (through Origin,
 * facing Normal), and how far, in units, the ground strays from that plane anywhere under it.
 */
struct FVeyraGroundFit
{
	FVector Origin = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;
	double MaxStray = 0.0;
};

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

	/**
	 * How much of a telegraph of Source shows (ADR-075 §1): what its ability's Effects chose, but only where an effect of
	 * theirs shows that area at its true shape; all of it for an ability without one, for a source that is no cast's (the
	 * player's own indicator, the attack range, the selection), or when the player turned Show Skill Outlines on. An
	 * opponent is never shown less than an effect shows them.
	 */
	VEYRAUI_API EVeyraTelegraphShow ShowOf(const FVeyraAbilityEffects* Effects, EVeyraTelegraphSource Source, bool bSkillOutlines);

	/** Points across Placed's footprint, on the ground plane: its origin, and its rim and middle (a circle's or sector's), or its length and breadth (a rectangle's). */
	VEYRAUI_API TArray<FVector2D> FootprintOf(const FVeyraPlacedShape& Placed);

	/** The plane that best fits the ground under Placed's footprint, HeightAt giving the ground's height at a point. */
	VEYRAUI_API FVeyraGroundFit FitGround(const FVeyraPlacedShape& Placed, TFunctionRef<double(const FVector2D&)> HeightAt);

	/**
	 * The fitted ground's slope (rise over run) along Direction on the ground and across it to its left, which an effect
	 * turned only by Direction's yaw lifts its particles by: so it lies on that plane, its footprint from above unchanged.
	 */
	VEYRAUI_API FVector2D SlopesOf(const FVeyraGroundFit& Fit, const FVector& Direction);

	/**
	 * What a telegraph shows over ground that strays from its best plane (ADR-075 §3): an effect drawn on one plane cannot
	 * follow ground that strays beyond Tolerance, so a hidden telegraph keeps its outline there, which follows the ground.
	 */
	VEYRAUI_API EVeyraTelegraphShow ShowOverGround(EVeyraTelegraphShow Show, const FVeyraGroundFit& Fit, double Tolerance);
}
