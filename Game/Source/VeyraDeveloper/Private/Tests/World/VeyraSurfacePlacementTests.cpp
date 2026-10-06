// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER

#include "Components/ActorTestSpawner.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Terrain/VeyraGround.h"
#include "Terrain/VeyraSurfacePlacement.h"
#include "Tuning/VeyraWorldTuning.h"

namespace VeyraWorldTests
{
	// Veyra.World.SurfacePlacement.*: things stand on the playable ground (ADR-040 §4), whatever its height, never on a
	// wall standing on it, and never at an assumed height where there is none.
	TEST_CLASS(SurfacePlacement, "Veyra.World")
	{
		FActorTestSpawner Spawner;

		// Fixture values: the search's bounds, a body, and the slabs it stands on.
		static constexpr double BodyHalfHeight = 88.0;
		static constexpr double SlabHalfExtent = 500.0;
		static constexpr double SlabHalfThickness = 50.0;
		static constexpr double RaisedTo = 350.0;
		static constexpr double WallAbove = 300.0;
		static constexpr double Slack = 0.01;

		static FVeyraSurfaceTuning Settings()
		{
			FVeyraSurfaceTuning Result;
			Result.MinZ = -1000.0;
			Result.MaxZ = 2000.0;
			Result.MaxSlopeDegrees = 40.0;
			return Result;
		}

		/** A slab centred at Height: playable ground, or a wall on WorldStatic. */
		UBoxComponent& SpawnSlab(double Height, bool bGround)
		{
			AActor* Slab = Spawner.GetWorld().SpawnActor<AActor>();
			UBoxComponent* Box = NewObject<UBoxComponent>(Slab);
			Slab->SetRootComponent(Box);
			Box->SetBoxExtent(FVector(SlabHalfExtent, SlabHalfExtent, SlabHalfThickness));
			if (bGround)
			{
				VeyraGround::MakeGround(*Box);
			}
			else
			{
				Box->SetCollisionObjectType(ECC_WorldStatic);
				Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				Box->SetCollisionResponseToAllChannels(ECR_Block);
			}
			Box->RegisterComponent();
			Slab->SetActorLocation(FVector(0.0, 0.0, Height));
			return *Box;
		}

		TEST_METHOD(RaisedGroundDeterminesCapsuleHeight)
		{
			SpawnSlab(RaisedTo, true);
			FVector Location;
			ASSERT_THAT(IsTrue(VeyraSurfacePlacement::Resolve(Spawner.GetWorld(), FVector2D::ZeroVector, BodyHalfHeight, Settings(), Location)));
			ASSERT_THAT(IsTrue(Location.Equals(FVector(0.0, 0.0, RaisedTo + SlabHalfThickness + BodyHalfHeight), Slack)));
		}

		TEST_METHOD(AWallOnTheGroundIsNoPlaceToStand)
		{
			SpawnSlab(0.0, true);
			SpawnSlab(WallAbove, false);
			FVector Location;
			ASSERT_THAT(IsTrue(VeyraSurfacePlacement::Resolve(Spawner.GetWorld(), FVector2D::ZeroVector, BodyHalfHeight, Settings(), Location)));
			ASSERT_THAT(IsTrue(Location.Equals(FVector(0.0, 0.0, SlabHalfThickness + BodyHalfHeight), Slack), TEXT("on the ground under the wall")));
		}

		TEST_METHOD(MissingGroundDoesNotChangeTheOutput)
		{
			SpawnSlab(0.0, false);
			FVector Location(1.0, 2.0, 3.0);
			ASSERT_THAT(IsFalse(VeyraSurfacePlacement::Resolve(Spawner.GetWorld(), FVector2D::ZeroVector, BodyHalfHeight, Settings(), Location)));
			ASSERT_THAT(IsTrue(Location.Equals(FVector(1.0, 2.0, 3.0))));
		}

		TEST_METHOD(InvalidBoundsAndBodyAreRejected)
		{
			SpawnSlab(0.0, true);
			FVeyraSurfaceTuning Invalid = Settings();
			Invalid.MinZ = Invalid.MaxZ;
			FVector Location;
			ASSERT_THAT(IsFalse(VeyraSurfacePlacement::Resolve(Spawner.GetWorld(), FVector2D::ZeroVector, BodyHalfHeight, Invalid, Location)));
			ASSERT_THAT(IsFalse(VeyraSurfacePlacement::Resolve(Spawner.GetWorld(), FVector2D::ZeroVector, -1.0, Settings(), Location)));
		}

		TEST_METHOD(StandingAtFollowsTheGroundAndKeepsTheHeightWithoutIt)
		{
			const FVector Above(0.0, 0.0, RaisedTo);
			ASSERT_THAT(IsTrue(VeyraGround::StandingAt(Spawner.GetWorld(), Above, BodyHalfHeight).Equals(Above), TEXT("no ground, no change")));
			SpawnSlab(-RaisedTo, true);
			const FVector Standing = VeyraGround::StandingAt(Spawner.GetWorld(), Above, BodyHalfHeight);
			ASSERT_THAT(IsTrue(Standing.Equals(FVector(0.0, 0.0, -RaisedTo + SlabHalfThickness + BodyHalfHeight), Slack)));
		}
	};
}

#endif
