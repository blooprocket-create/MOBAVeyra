// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Greybox/VeyraSpringChain.h"

namespace
{
	/** Point moved out of Collider onto its surface, if it is inside. */
	FVector OutOf(const FVector& Point, const VeyraSpringChain::FCollider& Collider)
	{
		const FVector Segment = Collider.B - Collider.A;
		const double Length = Segment.SizeSquared();
		const double Along = Length > UE_KINDA_SMALL_NUMBER ? FMath::Clamp(FVector::DotProduct(Point - Collider.A, Segment) / Length, 0.0, 1.0) : 0.0;
		const FVector Nearest = Collider.A + Segment * Along;
		const FVector Away = Point - Nearest;
		const double Distance = Away.Size();
		if (Distance >= Collider.Radius || Distance <= UE_KINDA_SMALL_NUMBER)
		{
			return Point;
		}
		return Nearest + Away / Distance * Collider.Radius;
	}

	/** Direction turned toward Toward until it is at most MaxRadians from it. */
	FVector WithinAngle(const FVector& Direction, const FVector& Toward, double MaxRadians)
	{
		const FVector From = Direction.GetSafeNormal();
		const FVector To = Toward.GetSafeNormal();
		const double Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(From, To), -1.0, 1.0));
		if (Angle <= MaxRadians || Angle <= UE_KINDA_SMALL_NUMBER)
		{
			return From;
		}
		// Back toward the clip's bone along the great circle between them, to the limit.
		const FQuat Turn = FQuat::FindBetweenNormals(To, From);
		const FQuat Limited = FQuat::Slerp(FQuat::Identity, Turn, MaxRadians / Angle);
		return Limited.RotateVector(To);
	}
}

VeyraSpringChain::FState VeyraSpringChain::AtRest(TConstArrayView<FVector> Animated)
{
	FState State;
	State.Points = TArray<FVector>(Animated.GetData(), Animated.Num());
	State.Previous = State.Points;
	State.Animated = State.Points;
	return State;
}

void VeyraSpringChain::Step(FState& State, TConstArrayView<FVector> Animated, float DeltaSeconds, const FParams& Params,
	TConstArrayView<FCollider> Colliders, const FTiming& Timing)
{
	const int32 Count = Animated.Num();
	if (Count < 2)
	{
		return;
	}
	if (State.Points.Num() != Count || State.Previous.Num() != Count || State.Animated.Num() != Count
		|| FVector::Dist(State.Animated[0], Animated[0]) > Timing.TeleportDistance)
	{
		State = AtRest(Animated);
		return;
	}
	const double Seconds = FMath::Clamp(static_cast<double>(DeltaSeconds), 0.0, static_cast<double>(Timing.MaxStepSeconds));
	if (Seconds <= 0.0)
	{
		return;
	}
	// Whole substeps, the last one no longer than the rest (a frame of exactly n substeps is n, not n + 1).
	const int32 Substeps = Timing.SubstepSeconds > 0.0f
		? FMath::Max(1, FMath::CeilToInt32(Seconds / Timing.SubstepSeconds - UE_KINDA_SMALL_NUMBER))
		: 1;
	const double Substep = Seconds / Substeps;
	// Frame-rate independent: what share of a joint's speed through the air, and of its speed relative to its clip, it
	// keeps for a substep.
	const double KeepInAir = FMath::Exp(-static_cast<double>(Params.Drag) * Substep);
	const double KeepOnClip = FMath::Exp(-static_cast<double>(Params.Damping) * Substep);
	const double Pull = static_cast<double>(Params.Stiffness) * Substep * Substep;
	const double MaxRadians = FMath::DegreesToRadians(static_cast<double>(Params.MaxAngleDegrees));
	// The clip's joints this substep and the one before, moving evenly from last frame's pose to this one's.
	TArray<FVector, TInlineAllocator<8>> Clip(State.Animated);
	TArray<FVector, TInlineAllocator<8>> Was;
	for (int32 Sub = 1; Sub <= Substeps; ++Sub)
	{
		Was = Clip;
		const double Share = static_cast<double>(Sub) / Substeps;
		for (int32 Joint = 0; Joint < Count; ++Joint)
		{
			Clip[Joint] = FMath::Lerp(State.Animated[Joint], Animated[Joint], Share);
		}
		State.Points[0] = Clip[0];
		State.Previous[0] = Clip[0];
		for (int32 Joint = 1; Joint < Count; ++Joint)
		{
			const FVector At = State.Points[Joint];
			// Its speed relative to its clip damped, then its speed through the air dragged; then the spring's pull toward
			// where its clip was as the substep began (where it is itself), so the pull never leads it by a substep.
			const FVector ClipMoved = Clip[Joint] - Was[Joint];
			const FVector Moved = At - State.Previous[Joint];
			const FVector Kept = (ClipMoved + (Moved - ClipMoved) * KeepOnClip) * KeepInAir;
			FVector Next = At + Kept + (Was[Joint] - At) * Pull;
			// Each bone its clip's length, within its turn of the clip's bone, outside the body; its length again.
			const FVector Root = State.Points[Joint - 1];
			const FVector Bone = Clip[Joint] - Clip[Joint - 1];
			const double Length = Bone.Size();
			FVector Direction = WithinAngle(Next - Root, Bone, MaxRadians);
			Next = Root + Direction * Length;
			for (const FCollider& Collider : Colliders)
			{
				Next = OutOf(Next, Collider);
			}
			Direction = (Next - Root).GetSafeNormal();
			State.Previous[Joint] = At;
			State.Points[Joint] = Root + (Direction.IsNearlyZero() ? Bone.GetSafeNormal() : Direction) * Length;
		}
	}
	State.Animated = TArray<FVector>(Animated.GetData(), Count);
}
