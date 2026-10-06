// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Terrain/VeyraSurfacePlacement.h"

#include "Engine/World.h"
#include "Terrain/VeyraGround.h"
#include "Tuning/VeyraWorldTuning.h"

namespace VeyraSurfacePlacement
{
	bool Resolve(const UWorld& World, const FVector2D& Point, double HalfHeight, const FVeyraSurfaceTuning& Settings, FVector& OutLocation)
	{
		if (!FMath::IsFinite(HalfHeight) || HalfHeight < 0.0 || !FMath::IsFinite(Settings.MaxSlopeDegrees) || Settings.MaxSlopeDegrees < 0.0
			|| Settings.MaxSlopeDegrees >= 90.0)
		{
			return false;
		}
		// Only ground answers: a wall's top is no place to stand (ADR-043), and the ground's channel holds no walls.
		FHitResult Hit;
		if (!VeyraGround::Find(World, Point, Settings.MaxZ, Settings.MinZ, Hit)
			|| Hit.ImpactNormal.Z < FMath::Cos(FMath::DegreesToRadians(Settings.MaxSlopeDegrees)))
		{
			return false;
		}
		OutLocation = Hit.ImpactPoint + FVector::UpVector * HalfHeight;
		return true;
	}
}
