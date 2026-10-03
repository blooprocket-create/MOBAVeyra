// Copyright © 2026 Wayfinder Studios. All rights reserved.
#pragma once
#include "CoreMinimal.h"

struct FVeyraWorldTuning;
struct FVeyraTerrainTuning;

struct FVeyraRiverSample
{
	FVector2D Point = FVector2D::ZeroVector;
	double Width = 0.0;
};

/** Source geometry shared by authoring, river classification and validation (ADR-040). */
namespace VeyraTerrainProfile
{
	VEYRAWORLD_API TArray<FVeyraRiverSample> River(const FVeyraTerrainTuning& Terrain, bool bMirror);
	/** Signed distance to either branch's bank: negative in the river. */
	VEYRAWORLD_API double RiverDistance(const FVeyraTerrainTuning& Terrain, const FVector2D& Point);
	VEYRAWORLD_API double Height(const FVeyraWorldTuning& Tuning, const FVector2D& Point);
	/** Dressing clearance measured against the authoritative layout and actor footprints. */
	VEYRAWORLD_API bool AllowsDressing(const FVeyraWorldTuning& Tuning, const FVector2D& Point, double Radius);
}

/** Prepared query for dense authoring grids: samples each river branch once. */
class VEYRAWORLD_API FVeyraTerrainSampler
{
public:
	explicit FVeyraTerrainSampler(const FVeyraWorldTuning& InTuning);
	double RiverDistance(const FVector2D& Point) const;
	double Height(const FVector2D& Point) const;
private:
	const FVeyraWorldTuning& Tuning;
	TArray<FVeyraRiverSample> BranchA;
	TArray<FVeyraRiverSample> BranchB;
};
