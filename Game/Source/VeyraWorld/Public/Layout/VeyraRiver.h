// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/VeyraWidthCurve.h"

struct FVeyraBattlegroundLayout;
struct FVeyraRiverLayout;

/** One sample of a river channel's centreline: where it passes and how wide its water is there. */
using FVeyraRiverSample = FVeyraCurveSample;

/** One channel of the whole river, sampled densely along its centreline (ADR-040 §6). */
struct FVeyraRiverChannel
{
	/** "Main", or "Island" with its Flux Well site, for the side channel around that Well. */
	FName Id;

	/** For an island's channel, the Flux Well site it rounds; none for the main channel. */
	int32 Site = INDEX_NONE;

	TArray<FVeyraRiverSample> Samples;
};

/**
 * The river as both teams share it (ADR-040 §6): Team A's authored main channel joined at the centre to its rotation and
 * sampled as one curve, and each island's side channel with its rotation. The geometry gameplay classifies, the terrain
 * field carves and the river's presentation draws.
 */
class VEYRAWORLD_API FVeyraRiverShape
{
public:
	explicit FVeyraRiverShape(const FVeyraRiverLayout& River);

	const TArray<FVeyraRiverChannel>& GetChannels() const { return Channels; }

	/** How far Point lies from the nearest water's edge: negative in the water. */
	double SignedDistance(const FVector2D& Point) const;

	/** As SignedDistance, against one channel. */
	static double SignedDistance(const FVeyraRiverChannel& Channel, const FVector2D& Point);

	bool IsWater(const FVector2D& Point) const { return SignedDistance(Point) <= 0.0; }

	/**
	 * Whether Point is enclosed by water within Reach in every one of Directions directions: on an island. Only a point
	 * on dry land can be.
	 */
	bool IsOnIsland(const FVector2D& Point, double Reach, int32 Directions, double Step) const;

private:
	TArray<FVeyraRiverChannel> Channels;
};

namespace VeyraRiver
{
	/** Point turned half a turn about the battleground's centre: Team A's half to Team B's. */
	inline FVector2D Rotate(const FVector2D& Point) { return -Point; }

	/**
	 * The layout's river, built once for each distinct river and kept, so classification is cheap at runtime. Valid for
	 * as long as the program runs.
	 */
	VEYRAWORLD_API const FVeyraRiverShape& ShapeOf(const FVeyraBattlegroundLayout& Layout);
}
