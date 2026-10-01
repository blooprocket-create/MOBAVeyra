// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Terrain/VeyraTerrainSubsystem.h"

#include "CollisionQueryParams.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Terrain/VeyraTerrainWall.h"
#include "Units/VeyraUnit.h"
#include "VeyraCombatVerbs.h"
#include "VeyraWorldLog.h"

namespace
{
	/** How far past touching a moved unit stands, so its body meets the wall's face rather than overlapping it: geometry, not tuning. */
	constexpr double ClearanceUnits = 1.0;
}

void UVeyraTerrainSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UVeyraRuntimeTerrainRegistry* Registry = Collection.InitializeDependency<UVeyraRuntimeTerrainRegistry>())
	{
		Registry->Register(*this);
	}
}

void UVeyraTerrainSubsystem::Deinitialize()
{
	if (UVeyraRuntimeTerrainRegistry* Registry = GetWorld() ? GetWorld()->GetSubsystem<UVeyraRuntimeTerrainRegistry>() : nullptr)
	{
		Registry->Unregister(*this);
	}
	Walls.Reset();
	Super::Deinitialize();
}

int32 UVeyraTerrainSubsystem::RaiseWall(const FVeyraWallRequest& Request)
{
	UWorld* World = GetWorld();
	const FVector Facing = Request.Facing.GetSafeNormal2D();
	if (!World || World->GetNetMode() == NM_Client || Facing.IsNearlyZero() || !(Request.Length > 0.0) || !(Request.Thickness > 0.0) || !(Request.HalfHeight > 0.0))
	{
		UE_LOG(LogVeyraWorld, Error, TEXT("Refused a wall at %s: only the server raises one, facing somewhere, with a length, thickness and height above 0."),
			*Request.Centre.ToString());
		return 0;
	}
	const FTransform Where(Facing.Rotation(), Request.Centre);
	AVeyraTerrainWall* Wall = World->SpawnActorDeferred<AVeyraTerrainWall>(AVeyraTerrainWall::StaticClass(), Where, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Wall)
	{
		return 0;
	}
	Wall->SetHalfExtent(FVector(Request.Thickness / 2.0, Request.Length / 2.0, Request.HalfHeight));
	Wall->FinishSpawning(Where);
	Wall->Form();
	MoveOut(Request);
	const int32 Handle = NextHandle++;
	Walls.Add(Handle, Wall);
	return Handle;
}

void UVeyraTerrainSubsystem::LowerWall(int32 Handle)
{
	TWeakObjectPtr<AVeyraTerrainWall> Wall;
	if (Walls.RemoveAndCopyValue(Handle, Wall) && Wall.IsValid())
	{
		Wall->Destroy();
	}
}

void UVeyraTerrainSubsystem::MoveOut(const FVeyraWallRequest& Request) const
{
	UWorld& World = *GetWorld();
	const FVector Facing = Request.Facing.GetSafeNormal2D();
	TArray<FOverlapResult> Overlaps;
	World.OverlapMultiByObjectType(Overlaps, Request.Centre, Facing.ToOrientationQuat(), FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeBox(FVector(Request.Thickness / 2.0, Request.Length / 2.0, Request.HalfHeight)));
	TSet<APawn*> Moved;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		APawn* Unit = Cast<APawn>(Overlap.GetActor());
		// Structures stand where the battleground built them; only what moves is moved.
		if (!Unit || Moved.Contains(Unit) || !VeyraUnits::KindOf(Unit).IsSet() || VeyraUnits::IsStructure(Unit))
		{
			continue;
		}
		Moved.Add(Unit);
		float Radius = 0.0f;
		float HalfHeight = 0.0f;
		Unit->GetSimpleCollisionCylinder(Radius, HalfHeight);
		// Out along the wall's facing, on the side its centre was on: the nearest legal point.
		const FVector Here = Unit->GetActorLocation();
		const double Along = FVector::DotProduct(Here - Request.Centre, Facing);
		const double Side = Along >= 0.0 ? 1.0 : -1.0;
		const FVector Out = Here + Facing * (Side * (Request.Thickness / 2.0 + Radius + ClearanceUnits) - Along);
		const FVector Ground = VeyraCombat::NearestGround(World, Out);
		Unit->TeleportTo(FVector(Ground.X, Ground.Y, Here.Z), Unit->GetActorRotation(), /*bIsATest*/ false, /*bNoCheck*/ true);
		UE_LOG(LogVeyraWorld, Verbose, TEXT("A wall formed on %s, which moves out to %s."), *GetNameSafe(Unit), *Unit->GetActorLocation().ToString());
	}
}
