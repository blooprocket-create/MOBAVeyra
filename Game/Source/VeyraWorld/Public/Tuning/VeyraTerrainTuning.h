// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "VeyraTerrainTuning.generated.h"

/** One control point of a river channel (ADR-040 §6): where its centreline passes and how wide its water is there. */
USTRUCT()
struct FVeyraRiverPoint
{
	GENERATED_BODY()

	UPROPERTY()
	double X = 0.0;

	UPROPERTY()
	double Y = 0.0;

	/** The water's full width at this point, in world units. */
	UPROPERTY()
	double Width = 0.0;
};

/**
 * A side channel that leaves the main channel and rejoins it around a Flux Well, making the Well's island (author ruling
 * 2026-10-05). Its ends lie in the main channel's water. Authored for one island; the other is its rotation.
 */
USTRUCT()
struct FVeyraRiverIsland
{
	GENERATED_BODY()

	/** The Flux Well site (an index into fluxWells.sites) the island carries. */
	UPROPERTY()
	int32 Site = 0;

	UPROPERTY()
	TArray<FVeyraRiverPoint> Channel;
};

/**
 * The river both teams share (ADR-040 §6). Team A's half is authored; Team B's is its rotation about the battleground's
 * centre, so the whole river is a naturally curved line through the centre whose two halves match. The main channel runs
 * from the centre outward, past the floor's edge; its rotation continues it the other way, and both are sampled as one
 * curve. Each island's side channel is sampled with its rotation.
 */
USTRUCT()
struct FVeyraRiverLayout
{
	GENERATED_BODY()

	/** The main channel's controls, from the centre (the first must be 0, 0) outward. */
	UPROPERTY()
	TArray<FVeyraRiverPoint> Main;

	UPROPERTY()
	TArray<FVeyraRiverIsland> Islands;

	/** How finely each span between controls is sampled into the channel's shape. */
	UPROPERTY()
	int32 SamplesPerSegment = 0;

	/** The water's surface and the riverbed under it, in world units. Units wade on the bed. */
	UPROPERTY()
	double SurfaceZ = 0.0;

	UPROPERTY()
	double BedZ = 0.0;

	/** How fast the water is drawn flowing, in units a second. Presentation only. */
	UPROPERTY()
	double FlowSpeed = 0.0;
};

/**
 * The battleground's ground levels (ADR-040 §3): heights and the runs between them, which decide its slopes and so how it
 * is walked. Gameplay-significant, so authored here; the shape between them is composed by the terrain field.
 */
USTRUCT()
struct FVeyraTerrainTuning
{
	GENERATED_BODY()

	/** Each lane's road, each base's pad, the jungle's shelves, the Wells' islands and the ridges walls stand on. */
	UPROPERTY()
	double LaneZ = 0.0;

	UPROPERTY()
	double BaseZ = 0.0;

	UPROPERTY()
	double JungleZ = 0.0;

	UPROPERTY()
	double IslandZ = 0.0;

	UPROPERTY()
	double RidgeZ = 0.0;

	/** How far beside a lane's road the ground stays at the road's level. */
	UPROPERTY()
	double LaneShoulder = 0.0;

	/** The run over which ground climbs from a road, a pad or a clearing to the jungle's height. */
	UPROPERTY()
	double JungleRise = 0.0;

	/** The run from the water's edge to the top of its bank. */
	UPROPERTY()
	double BankWidth = 0.0;

	/** Under the water, the share of a bank's run over which the bed rises to the waterline: the shore's shelf. */
	UPROPERTY()
	double UnderwaterShelfShare = 0.0;

	/** The share of a camp's leash radius that stays level as its clearing, and the share beyond it over which the ground returns. */
	UPROPERTY()
	double ClearingCoreShare = 0.0;

	UPROPERTY()
	double ClearingFadeShare = 0.0;

	/** The run of a ridge's cliff face, rising from its wall's edge inward: the whole cliff stands within the wall. */
	UPROPERTY()
	double RidgeSkirt = 0.0;

	/** The rim beyond the floor's edge: its height and the run of its face. */
	UPROPERTY()
	double BoundaryZ = 0.0;

	UPROPERTY()
	double BoundaryWidth = 0.0;

	/** How far a wall's collision reaches below the lowest ground under it. */
	UPROPERTY()
	double WallFootingClearance = 0.0;
};
