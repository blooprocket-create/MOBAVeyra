// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/VeyraRiver.h"
#include "Terrain/VeyraTerrainBox.h"

struct FVeyraWorldTuning;

/**
 * The jungle's gentle relief, a presentation choice (ADR-040 §5: style profiles hold non-spatial parameters): how far it
 * rises and falls, and how broad its swells are. It is made symmetric under the battleground's rotation, so both
 * teams' ground matches. None by default.
 */
struct FVeyraTerrainRelief
{
	double Amplitude = 0.0;
	double Wavelength = 0.0;

	/** How far a ridge's crest rises and falls along it. */
	double CrestAmplitude = 0.0;

	int32 Seed = 0;
};

/** What the ground is at a point, as the terrain field composes it. */
struct FVeyraTerrainSample
{
	double Height = 0.0;

	/** How far the point is from the nearest water's edge: negative in the water. */
	double WaterDistance = 0.0;

	/** From 0 to 1: how much of a lane's road, a base's pad and a ridge's rock the ground is here. */
	double Road = 0.0;
	double Pad = 0.0;
	double Ridge = 0.0;

	/** From 0 to 1: how far beyond the floor's edge the rim has risen. */
	double Rim = 0.0;
};

/**
 * The battleground's ground as a pure function of its layout (ADR-040 §3): the lanes' roads level, the ground climbing
 * from them and the bases to the jungle's shelves, a ridge raised on every wall, the river's water cut into a basin with
 * walkable banks, each Flux Well's island a low platform, and a rim beyond the floor's edge, open where the river leaves.
 * Every input is the layout's, so the ground is symmetric under the battleground's rotation. World authoring samples it
 * to build the terrain; gameplay never does, and stands on the terrain built.
 */
class VEYRAWORLD_API FVeyraTerrainField
{
public:
	explicit FVeyraTerrainField(const FVeyraWorldTuning& Tuning, const FVeyraTerrainRelief& Relief = FVeyraTerrainRelief());

	FVeyraTerrainSample Sample(const FVector2D& Point) const;

	double Height(const FVector2D& Point) const { return Sample(Point).Height; }

private:
	/** The ground's height before the river and the ridges: the jungle's shelves, the lanes' roads, the bases' pads and the islands. */
	double Ground(const FVector2D& Point, FVeyraTerrainSample& Out) const;

	/** The relief at Point, the same at its rotation. */
	double ReliefAt(const FVector2D& Point, double Wavelength, double Amplitude) const;

	const FVeyraWorldTuning& Tuning;
	FVeyraTerrainRelief Relief;
	const FVeyraRiverShape& River;
	TArray<FVeyraTerrainBox> Walls;
	TArray<FVector2D> Pads;
	TArray<TPair<FVector2D, double>> Clearings;
	TArray<FVector2D> Wells;
};
