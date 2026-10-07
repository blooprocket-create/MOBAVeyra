// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Greybox/VeyraLimbIK.h"

VeyraLimbIK::FChain VeyraLimbIK::Solve(const FChain& Chain, const FVector& Target, const FVector& Pole, float MaxStretch)
{
	const float Upper = FVector::Dist(Chain.Root, Chain.Joint);
	const float Lower = FVector::Dist(Chain.Joint, Chain.End);
	const FVector ToTarget = Target - Chain.Root;
	const float Distance = ToTarget.Size();
	if (Upper <= KINDA_SMALL_NUMBER || Lower <= KINDA_SMALL_NUMBER || Distance <= KINDA_SMALL_NUMBER)
	{
		return Chain;
	}
	const FVector Along = ToTarget / Distance;
	// Beyond reach, each bone gives up to MaxStretch of its length before the end stops short.
	const float Stretch = FMath::Clamp(Distance / (Upper + Lower), 1.0f, 1.0f + FMath::Max(MaxStretch, 0.0f));
	const float A = Upper * Stretch;
	const float B = Lower * Stretch;
	const float Reach = FMath::Min(Distance, (A + B) * 0.9999f);
	// The angle at the root, by the law of cosines.
	const float Cosine = FMath::Clamp((A * A + Reach * Reach - B * B) / (2.0f * A * Reach), -1.0f, 1.0f);
	// Which way the joint bends: toward the pole, square to the line from root to target; failing that, as it bent.
	FVector Bend = (Pole - Chain.Root) - Along * FVector::DotProduct(Pole - Chain.Root, Along);
	if (!Bend.Normalize())
	{
		Bend = (Chain.Joint - Chain.Root) - Along * FVector::DotProduct(Chain.Joint - Chain.Root, Along);
		if (!Bend.Normalize())
		{
			Bend = FVector::CrossProduct(Along, FVector::UpVector).GetSafeNormal();
		}
	}
	FChain Solved;
	Solved.Root = Chain.Root;
	Solved.Joint = Chain.Root + Along * (Cosine * A) + Bend * (FMath::Sqrt(FMath::Max(0.0f, 1.0f - Cosine * Cosine)) * A);
	Solved.End = Chain.Root + Along * Reach;
	return Solved;
}

float VeyraLimbIK::PlantWeight(float LiftAboveRest, float FadeHeight)
{
	if (FadeHeight <= 0.0f)
	{
		return LiftAboveRest <= 0.0f ? 1.0f : 0.0f;
	}
	const float Share = FMath::Clamp(LiftAboveRest / FadeHeight, 0.0f, 1.0f);
	// Eased, so a foot leaves the ground and lands without a pop.
	return 1.0f - Share * Share * (3.0f - 2.0f * Share);
}

float VeyraLimbIK::PelvisDrop(float LeftOffset, float LeftWeight, float RightOffset, float RightWeight, float MaxDrop)
{
	const float Lowest = FMath::Min(LeftOffset * LeftWeight, RightOffset * RightWeight);
	return FMath::Clamp(Lowest, -FMath::Abs(MaxDrop), 0.0f);
}

float VeyraLimbIK::HoldWeight(float Distance, float ReleaseDistance)
{
	// Eased like a plant: lifting a foot and taking a hand off a grip are the same release.
	return PlantWeight(Distance, ReleaseDistance);
}

float VeyraLimbIK::SpeedWeight(float Speed, float FullSpeed, float MovingWeight)
{
	const float Share = FullSpeed > 0.0f ? FMath::Clamp(Speed / FullSpeed, 0.0f, 1.0f) : 0.0f;
	return FMath::Lerp(1.0f, MovingWeight, Share);
}
