// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Math/Vector.h"
#include "Math/Vector2D.h"
#include "Misc/Optional.h"

#include "VeyraCameraRules.generated.h"

/** How the local camera moves (Settings Bible §2; ADR-020 §1). */
UENUM()
enum class EVeyraCameraMode : uint8
{
	/** It stays where the player puts it: edge scrolling, the camera keys and middle-mouse drag. */
	Free,
	/** It follows the Vanguard. */
	Locked,
	/** It follows the Vanguard, and the player may look a limited way off it. */
	SemiLocked,
};

/** What moves the camera this frame. */
struct FVeyraCameraInput
{
	/** From the camera keys, each axis -1 to 1: X to the screen's right, Y to its top. */
	FVector2D Pan = FVector2D::ZeroVector;

	/** From the screen's edges, the same way; it moves at its own speed (Settings Bible §12.3). */
	FVector2D EdgePan = FVector2D::ZeroVector;

	/** World units dragged with the middle mouse button this frame, along the screen's right and top. */
	FVector2D Drag = FVector2D::ZeroVector;

	/** Hold to Center: follow the Vanguard while held, whatever the mode. */
	bool bHoldCenter = false;

	/** Where the Vanguard stands, if it has one. */
	TOptional<FVector> Vanguard;
};

/** Where the camera looks, and how far off the Vanguard a Semi-Locked camera has been pushed. */
struct FVeyraCameraState
{
	FVector Focus = FVector::ZeroVector;
	FVector Offset = FVector::ZeroVector;
};

/** The camera's presentation settings the rules read (UVeyraCameraSettings). */
struct FVeyraCameraLimits
{
	/** The camera keys' and the screen edges' pan speeds at full tilt, units per second. */
	double PanSpeed = 0.0;
	double EdgeScrollSpeed = 0.0;

	/** How far off the Vanguard Semi-Locked may look, in units. */
	double SemiLockedMaxOffset = 0.0;

	/** The focus stays within this far of the map's centre on each axis, in units. */
	double HalfExtent = 0.0;
};

/**
 * The pure rules of the local camera (ADR-020 §1). The view looks down the world's +X from the south,
 * so the screen's top is +X and its right is +Y. The camera never grants sight: it only chooses where
 * the player looks.
 */
namespace VeyraCamera
{
	/**
	 * Moves the camera for one frame of DeltaSeconds: Free goes where it is taken; Locked, or any mode
	 * with Hold to Center, stays on the Vanguard; Semi-Locked follows the Vanguard at the offset the
	 * player pushes it to, up to its limit. The focus never leaves the map.
	 */
	VEYRAMATCH_API FVeyraCameraState Step(const FVeyraCameraState& State, EVeyraCameraMode Mode, const FVeyraCameraInput& Input, const FVeyraCameraLimits& Limits, double DeltaSeconds);

	/** The mode the lock key moves to next: Free, Locked, Semi-Locked, then Free again. */
	VEYRAMATCH_API EVeyraCameraMode Next(EVeyraCameraMode Mode);

	/**
	 * The pan from the screen's edges: -1 or 1 on an axis where Mouse is within EdgePixels of that edge
	 * of a Viewport-sized screen, 0 elsewhere. Screen Y grows down; the result's Y grows up.
	 */
	VEYRAMATCH_API FVector2D EdgePan(const FVector2D& Mouse, const FVector2D& Viewport, double EdgePixels);

	/** A screen-space pan or drag as world units along the ground: the screen's top is +X, its right +Y. */
	VEYRAMATCH_API FVector ScreenToGround(const FVector2D& Screen);

	/**
	 * Where a pan from From to To looks Elapsed seconds into its Seconds, easing in and out; at To from
	 * then on. The end-of-match pan to the fallen Prime Well (ADR-020 §1).
	 */
	VEYRAMATCH_API FVector PanToward(const FVector& From, const FVector& To, double Elapsed, double Seconds);
}
