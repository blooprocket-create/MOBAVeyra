// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CQTest.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Movement/VeyraMovementComponent.h"
#include "Terrain/VeyraGround.h"
#include "Tests/Combat/VeyraCombatTestHelpers.h"
#include "VeyraCombatVerbs.h"
#include "VeyraVanguardCharacter.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCombatTests
{
	// Veyra.Combat.Ground.*: forced movement over real ground (ADR-040 §4). Rising ground never stops a path, as a wall
	// does; a path's end and a blink's landing stand on the ground there, however far it rises or falls.
	TEST_CLASS(Ground, "Veyra.Combat")
	{
		// Fixture values: low ground behind and under the body, a terrace rising ahead of it along +X, and a path.
		static constexpr double TerraceFromX = 200.0;
		static constexpr double GroundEndX = 2000.0;
		static constexpr double TerraceRise = 150.0;
		static constexpr double SlabHalfThickness = 50.0;
		static constexpr double SlabHalfWidth = 1000.0;
		static constexpr double Distance = 600.0;
		static constexpr double WallFaceX = 500.0;
		static constexpr double WallThickness = 50.0;
		static constexpr double WallHeight = 600.0;
		static constexpr double Tolerance = 2.0;

		FActorTestSpawner Spawner;
		UAbilitySystemComponent* Unit = nullptr;
		UVeyraMovementComponent* Movement = nullptr;
		FVector Start = FVector::ZeroVector;
		double HalfHeight = 0.0;

		BEFORE_EACH()
		{
			SpawnGround(-GroundEndX, TerraceFromX, 0.0);
			SpawnGround(TerraceFromX, GroundEndX, TerraceRise);
			Unit = &SpawnCombatant(Spawner);
			ASSERT_THAT(IsTrue(VeyraCombat::InitializeStats(*Unit, ExampleStats())));
			AVeyraVanguardCharacter& Body = Spawner.SpawnActorAt<AVeyraVanguardCharacter>(FVector::ZeroVector, FRotator::ZeroRotator);
			HalfHeight = Body.GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			Body.SetActorLocation(FVector(0.0, 0.0, HalfHeight));
			Body.SetPlayerState(CastChecked<APlayerState>(Unit->GetOwner()));
			Movement = Body.GetVeyraMovement();
			Movement->SetMovementMode(MOVE_Walking);
			Start = Body.GetActorLocation();
		}

		/** Playable ground from FromX to ToX along X, its top at TopZ. */
		void SpawnGround(double FromX, double ToX, double TopZ)
		{
			AActor* Slab = Spawner.GetWorld().SpawnActor<AActor>();
			UBoxComponent* Box = NewObject<UBoxComponent>(Slab);
			Slab->SetRootComponent(Box);
			Box->SetBoxExtent(FVector((ToX - FromX) / 2.0, SlabHalfWidth, SlabHalfThickness));
			VeyraGround::MakeGround(*Box);
			Box->RegisterComponent();
			Slab->SetActorLocation(FVector((FromX + ToX) / 2.0, 0.0, TopZ - SlabHalfThickness));
		}

		/** A wall across the path at WallFaceX, standing on the terrace and reaching below it. */
		void SpawnWall()
		{
			UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
			const FVector Size(WallThickness, SlabHalfWidth, WallHeight);
			const FTransform Transform(FRotator::ZeroRotator, FVector(WallFaceX + WallThickness / 2.0, 0.0, WallHeight / 2.0), Size / Cube->GetBoundingBox().GetSize());
			AStaticMeshActor* Wall = Spawner.GetWorld().SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform);
			Wall->SetMobility(EComponentMobility::Movable);
			Wall->GetStaticMeshComponent()->SetStaticMesh(Cube);
		}

		double Radius() const
		{
			return CastChecked<ACharacter>(Movement->GetOwner())->GetCapsuleComponent()->GetScaledCapsuleRadius();
		}

		TEST_METHOD(RisingGroundDoesNotStopAPathWhichEndsStandingOnIt)
		{
			bool bStopped = true;
			const FVector End = Movement->ResolveForcedMoveEnd(FVector::ForwardVector, Distance, &bStopped);
			ASSERT_THAT(IsFalse(bStopped, TEXT("ground is not terrain that stops a path")));
			ASSERT_THAT(IsTrue(End.Equals(FVector(Distance, 0.0, TerraceRise + Start.Z), Tolerance), *End.ToString()));
		}

		TEST_METHOD(AWallOnTheGroundStillStopsIt)
		{
			SpawnWall();
			bool bStopped = false;
			const FVector End = Movement->ResolveForcedMoveEnd(FVector::ForwardVector, Distance, &bStopped);
			ASSERT_THAT(IsTrue(bStopped));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(End.X, WallFaceX - Radius(), Tolerance), FString::Printf(TEXT("ended at X %g"), End.X)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(End.Z, TerraceRise + Start.Z, Tolerance), TEXT("on the terrace, against the wall")));
		}

		TEST_METHOD(FallingGroundLowersTheEnd)
		{
			AActor& Body = *Movement->GetOwner();
			Body.SetActorLocation(FVector(Distance, 0.0, TerraceRise + HalfHeight));
			const FVector End = Movement->ResolveForcedMoveEnd(FVector::BackwardVector, Distance);
			ASSERT_THAT(IsTrue(End.Equals(FVector(0.0, 0.0, HalfHeight), Tolerance), *End.ToString()));
		}

		TEST_METHOD(ABlinkLandsOnTheGroundWhereItEnds)
		{
			ASSERT_THAT(IsTrue(VeyraCombat::Blink(*Unit, FVector(Distance, 0.0, Start.Z), FVector::ForwardVector)));
			const FVector Landed = Movement->GetOwner()->GetActorLocation();
			ASSERT_THAT(IsTrue(Landed.Equals(FVector(Distance, 0.0, TerraceRise + Start.Z), Tolerance), *Landed.ToString()));
		}

		TEST_METHOD(APointCarriedElsewhereKeepsItsHeightAboveTheGround)
		{
			// Start stands on the low ground at 0, as high above it as its movement keeps it.
			const FVector Carried = VeyraGround::Carried(Spawner.GetWorld(), Start, FVector2D(Distance, 0.0));
			ASSERT_THAT(IsTrue(Carried.Equals(FVector(Distance, 0.0, TerraceRise + Start.Z), UE_KINDA_SMALL_NUMBER), *Carried.ToString()));
			const FVector Beyond = VeyraGround::Carried(Spawner.GetWorld(), Start, FVector2D(GroundEndX * 2.0, 0.0));
			ASSERT_THAT(IsTrue(Beyond.Equals(FVector(GroundEndX * 2.0, 0.0, Start.Z), Tolerance), TEXT("no ground there, the same height")));
		}
	};
}

#endif
