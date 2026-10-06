// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Terrain/VeyraGround.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"

FName VeyraGround::ProfileName()
{
	static const FName Name(TEXT("VeyraGround"));
	return Name;
}

void VeyraGround::MakeGround(UPrimitiveComponent& Component)
{
	Component.SetCollisionProfileName(ProfileName());
}

bool VeyraGround::Find(const UWorld& World, const FVector2D& Point, double TopZ, double BottomZ, FHitResult& OutHit)
{
	if (!FMath::IsFinite(Point.X) || !FMath::IsFinite(Point.Y) || !FMath::IsFinite(TopZ) || !FMath::IsFinite(BottomZ) || TopZ <= BottomZ)
	{
		return false;
	}
	const FCollisionQueryParams Query(SCENE_QUERY_STAT(VeyraGround), /*bTraceComplex*/ false);
	return World.LineTraceSingleByObjectType(OutHit, FVector(Point, TopZ), FVector(Point, BottomZ), FCollisionObjectQueryParams(Channel), Query)
		&& !OutHit.bStartPenetrating;
}

bool VeyraGround::Under(const UWorld& World, const FVector& Near, FVector& OutSurface)
{
	const double Reach = UVeyraCombatTuningSubsystem::Get().Ground.SearchHeight;
	FHitResult Hit;
	if (!Find(World, FVector2D(Near), Near.Z + Reach, Near.Z - Reach, Hit))
	{
		return false;
	}
	OutSurface = Hit.ImpactPoint;
	return true;
}

FVector VeyraGround::StandingAt(const UWorld& World, const FVector& Point, double HalfHeight)
{
	FVector Surface;
	return Under(World, Point, Surface) ? Surface + FVector::UpVector * HalfHeight : Point;
}

FVector VeyraGround::Carried(const UWorld& World, const FVector& From, const FVector2D& To)
{
	FVector Here;
	FVector There;
	if (!Under(World, From, Here) || !Under(World, FVector(To, From.Z), There))
	{
		return FVector(To, From.Z);
	}
	return There + FVector::UpVector * (From.Z - Here.Z);
}
