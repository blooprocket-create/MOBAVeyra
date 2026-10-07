// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * The inverse kinematics a body's drawn limbs are solved by (ADR-069): pure functions, used where a limb must hold to
 * something fixed (a foot on uneven ground, an off hand on a weapon) while its clips keep every free swing in forward
 * kinematics. Presentation only.
 */
namespace VeyraLimbIK
{
	/** A two-bone chain's joints, in one space: its root (a hip or shoulder), its joint (a knee or elbow) and its end. */
	struct FChain
	{
		FVector Root = FVector::ZeroVector;
		FVector Joint = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;
	};

	/**
	 * Chain solved so its end reaches Target: its root stays, its joint bends toward Pole (on the side of the root-to-
	 * target line that Pole is on, so a knee or elbow never flips), and each bone stretches by at most MaxStretch of
	 * its length where the target lies beyond reach, so the limb eases rather than locks. A target farther still is
	 * reached as far as the stretched chain goes.
	 */
	VEYRAUI_API FChain Solve(const FChain& Chain, const FVector& Target, const FVector& Pole, float MaxStretch);

	/** How planted a foot is in its clip, from 1 on its rest height to 0 lifted FadeHeight or more above it. */
	VEYRAUI_API float PlantWeight(float LiftAboveRest, float FadeHeight);

	/**
	 * How far the pelvis lowers so the lower of two feet reaches its ground: the most negative of the feet's weighted
	 * offsets (ground below the floor the body stands on), no lower than MaxDrop below; it never rises.
	 */
	VEYRAUI_API float PelvisDrop(float LeftOffset, float LeftWeight, float RightOffset, float RightWeight, float MaxDrop);

	/** How much of the solve a moving body keeps: 1 standing, falling toward MovingWeight as its speed reaches FullSpeed. */
	VEYRAUI_API float SpeedWeight(float Speed, float FullSpeed, float MovingWeight);

	/**
	 * How firmly a hand is held to its grip when its clip has it Distance away: 1 on the grip, easing to 0 at
	 * ReleaseDistance. A small drift (the carrying hand moved by a blend) is corrected; a clip that takes the hand away
	 * on purpose (a gesture, a reload) is let go.
	 */
	VEYRAUI_API float HoldWeight(float Distance, float ReleaseDistance);
}
