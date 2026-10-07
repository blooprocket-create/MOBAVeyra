// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Secondary motion for a drawn body's loose parts (ADR-069): a cloak, coat tails, a lock of hair hang on a chain of
 * bones that trails the clips' pose, as cloth and hair do. Pure functions over a chain's joints in world space, so a
 * body moving through the world drags its cloth behind it. Presentation only.
 */
namespace VeyraSpringChain
{
	/**
	 * How a chain moves. Each joint is a mass on a spring to where its clip has it: Stiffness is that spring's pull per
	 * centimetre away, per second squared (its natural frequency squared). Drag is how much of its speed through the air
	 * it loses per second, so a running body's cloak streams behind it; Damping is how much of its speed relative to its
	 * clip it loses per second, so a lock of hair settles without streaming. A bone turns at most MaxAngleDegrees from
	 * its clip's bone.
	 */
	struct FParams
	{
		float Stiffness = 0.0f;
		float Drag = 0.0f;
		float Damping = 0.0f;
		float MaxAngleDegrees = 0.0f;
	};

	/**
	 * How a chain keeps time: a frame longer than MaxStepSeconds (a hitch) counts as that long; the motion is stepped in
	 * substeps of at most SubstepSeconds, so it moves alike at any frame rate; a root farther than TeleportDistance from
	 * where it was (a respawn, a blink) settles the chain on its clip.
	 */
	struct FTiming
	{
		float MaxStepSeconds = 0.0f;
		float SubstepSeconds = 0.0f;
		float TeleportDistance = 0.0f;
	};

	/** A capsule a chain may not pass into: a segment and its radius (a torso, a thigh). */
	struct FCollider
	{
		FVector A = FVector::ZeroVector;
		FVector B = FVector::ZeroVector;
		float Radius = 0.0f;
	};

	/** A chain's joints as simulated (Points) and a substep ago (Previous), and its clip's joints the frame before
	 *  (Animated); its first joint is its root. */
	struct FState
	{
		TArray<FVector> Points;
		TArray<FVector> Previous;
		TArray<FVector> Animated;
	};

	/** The state at rest on Animated: still, where its clip has it. */
	VEYRAUI_API FState AtRest(TConstArrayView<FVector> Animated);

	/**
	 * Advances State by DeltaSeconds toward Animated (the clip's joints this frame, world space; the root is held there),
	 * the clip moving evenly from where it was across the substeps: each joint keeps its speed less its drag and its
	 * damping and is drawn toward its clip by its stiffness; then each bone keeps its clip's length, turns at most
	 * MaxAngleDegrees from the clip's bone, and is pushed out of every collider.
	 */
	VEYRAUI_API void Step(FState& State, TConstArrayView<FVector> Animated, float DeltaSeconds, const FParams& Params,
		TConstArrayView<FCollider> Colliders, const FTiming& Timing);
}
