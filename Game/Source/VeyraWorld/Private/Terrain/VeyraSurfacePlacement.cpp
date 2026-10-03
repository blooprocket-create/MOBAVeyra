// Copyright © 2026 Wayfinder Studios. All rights reserved.
#include "Terrain/VeyraSurfacePlacement.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Terrain/VeyraTerrainWall.h"
#include "Tuning/VeyraWorldTuning.h"

namespace VeyraSurfacePlacement
{
bool Resolve(UWorld& World, const FVector2D& Point, double HalfHeight,
	const FVeyraSurfaceTuning& Settings, FVector& OutLocation)
{
	if (!FMath::IsFinite(Point.X) || !FMath::IsFinite(Point.Y) || !FMath::IsFinite(HalfHeight) || HalfHeight < 0.0
		|| !FMath::IsFinite(Settings.MinZ) || !FMath::IsFinite(Settings.MaxZ) || Settings.MinZ >= Settings.MaxZ
		|| !FMath::IsFinite(Settings.MaxSlopeDegrees) || Settings.MaxSlopeDegrees < 0.0 || Settings.MaxSlopeDegrees >= 90.0)
	{
		return false;
	}
	FHitResult Hit;
	const FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(VeyraSurfacePlacement), false);
	// Walls block routes, but their tops are not a playable placement substrate (ADR-043).
	for (TActorIterator<AVeyraTerrainWall> It(&World); It; ++It)
	{
		Query.AddIgnoredActor(*It);
	}
	if (!World.LineTraceSingleByObjectType(Hit, FVector(Point, Settings.MaxZ), FVector(Point, Settings.MinZ), Objects, Query)
		|| Hit.bStartPenetrating || Hit.ImpactNormal.Z < FMath::Cos(FMath::DegreesToRadians(Settings.MaxSlopeDegrees)))
	{
		return false;
	}
	OutLocation = Hit.ImpactPoint + FVector::UpVector * HalfHeight;
	return true;
}
}
