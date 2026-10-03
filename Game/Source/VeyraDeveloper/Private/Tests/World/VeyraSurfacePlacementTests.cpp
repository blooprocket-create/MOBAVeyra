// Copyright © 2026 Wayfinder Studios. All rights reserved.
#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Terrain/VeyraSurfacePlacement.h"
#include "Tuning/VeyraWorldTuning.h"

#if WITH_AUTOMATION_WORKER
namespace VeyraWorldTests
{
TEST_CLASS(SurfacePlacement, "Veyra.World")
{
	FActorTestSpawner Spawner;

	static FVeyraSurfaceTuning Settings()
	{
		FVeyraSurfaceTuning Result;
		Result.MinZ = -1000.0;
		Result.MaxZ = 2000.0;
		Result.MaxSlopeDegrees = 40.0;
		return Result;
	}

	TEST_METHOD(RaisedGroundDeterminesCapsuleHeight)
	{
		UWorld& World = Spawner.GetWorld();
		AActor* Floor = World.SpawnActor<AActor>();
		UBoxComponent* Box = NewObject<UBoxComponent>(Floor);
		Floor->SetRootComponent(Box);
		Box->SetBoxExtent(FVector(500.0, 500.0, 50.0));
		Box->SetCollisionObjectType(ECC_WorldStatic);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->RegisterComponent();
		Floor->SetActorLocation(FVector(0.0, 0.0, 350.0));
		FVector Location;
		ASSERT_THAT(IsTrue(VeyraSurfacePlacement::Resolve(World, FVector2D::ZeroVector, 88.0, Settings(), Location)));
		ASSERT_THAT(IsTrue(Location.Equals(FVector(0.0, 0.0, 488.0), 0.01)));
	}

	TEST_METHOD(MissingGroundDoesNotChangeTheOutput)
	{
		FVector Location(1.0, 2.0, 3.0);
		ASSERT_THAT(IsFalse(VeyraSurfacePlacement::Resolve(Spawner.GetWorld(), FVector2D::ZeroVector, 88.0, Settings(), Location)));
		ASSERT_THAT(IsTrue(Location.Equals(FVector(1.0, 2.0, 3.0))));
	}

	TEST_METHOD(InvalidBoundsAndBodyAreRejected)
	{
		FVeyraSurfaceTuning Invalid = Settings();
		Invalid.MinZ = Invalid.MaxZ;
		FVector Location;
		ASSERT_THAT(IsFalse(VeyraSurfacePlacement::Resolve(Spawner.GetWorld(), FVector2D::ZeroVector, 88.0, Invalid, Location)));
		ASSERT_THAT(IsFalse(VeyraSurfacePlacement::Resolve(Spawner.GetWorld(), FVector2D::ZeroVector, -1.0, Settings(), Location)));
	}
};
}
#endif
