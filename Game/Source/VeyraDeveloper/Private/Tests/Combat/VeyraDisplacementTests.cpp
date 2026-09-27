// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CQTest.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Greybox/VeyraGreyboxLayout.h"
#include "Movement/VeyraMovementComponent.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Tests/Combat/VeyraCombatTestHelpers.h"
#include "VeyraCombatVerbs.h"
#include "VeyraVanguardCharacter.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCombatTests
{
	// Veyra.Combat.Displacement.*: how displacements and dashes are planned and take over a body
	// (Combat Bible §9; ADR-009 §2). Their movement over time is checked in Veyra.Net.Displacement.
	TEST_CLASS(Displacement, "Veyra.Combat")
	{
		// Fixture values: a wall in front of the body along +X, open floor behind it, and path values.
		static constexpr double WallFaceX = 300.0;
		static constexpr double WallThickness = 50.0;
		static constexpr double WallWidth = 600.0;
		static constexpr double WallHeight = 400.0;
		static constexpr double Distance = 400.0;
		static constexpr double Speed = 1000.0;
		static constexpr double Tolerance = 2.0;

		FActorTestSpawner Spawner;
		FVeyraGreyboxLayout Layout;
		UAbilitySystemComponent* Unit = nullptr;
		UAbilitySystemComponent* Enemy = nullptr;
		UVeyraMovementComponent* Movement = nullptr;
		FVector Start = FVector::ZeroVector;

		BEFORE_EACH()
		{
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			VeyraGreybox::SpawnFloor(Spawner.GetWorld(), Layout, EComponentMobility::Movable);
			SpawnWall();
			Unit = &SpawnCombatant(Spawner);
			Enemy = &SpawnCombatant(Spawner);
			ASSERT_THAT(IsTrue(VeyraCombat::InitializeStats(*Unit, ExampleStats())));
			AVeyraVanguardCharacter& Body = Spawner.SpawnActorAt<AVeyraVanguardCharacter>(FVector::ZeroVector, FRotator::ZeroRotator);
			Body.SetActorLocation(FVector(0.0, 0.0, Body.GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
			Body.SetPlayerState(CastChecked<APlayerState>(Unit->GetOwner()));
			Movement = Body.GetVeyraMovement();
			Movement->SetMovementMode(MOVE_Walking);
			Start = Body.GetActorLocation();
		}

		void SpawnWall()
		{
			UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
			const FVector Size(WallThickness, WallWidth, WallHeight);
			const FTransform Transform(FRotator::ZeroRotator, FVector(WallFaceX + WallThickness / 2.0, 0.0, WallHeight / 2.0), Size / Cube->GetBoundingBox().GetSize());
			AStaticMeshActor* Wall = Spawner.GetWorld().SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform);
			Wall->SetMobility(EComponentMobility::Movable);
			Wall->GetStaticMeshComponent()->SetStaticMesh(Cube);
		}

		double Radius() const
		{
			return CastChecked<ACharacter>(Movement->GetOwner())->GetCapsuleComponent()->GetScaledCapsuleRadius();
		}

		static FVeyraDisplacement KnockbackAlong(const FVector& Direction)
		{
			return FVeyraDisplacement{ Direction, Distance, Speed };
		}

		TEST_METHOD(TerrainStopsAForcedMoveWhereTheBodyFits)
		{
			const FVector End = Movement->ResolveForcedMoveEnd(FVector::ForwardVector, Distance * 2.0);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(End.X, WallFaceX - Radius(), Tolerance), FString::Printf(TEXT("ended at X %g"), End.X)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(End.Y, Start.Y) && FMath::IsNearlyEqual(End.Z, Start.Z)));
		}

		TEST_METHOD(OpenGroundTakesTheWholeDistance)
		{
			const FVector End = Movement->ResolveForcedMoveEnd(FVector::BackwardVector, Distance);
			ASSERT_THAT(IsTrue(FVector::Dist(End, Start - FVector(Distance, 0.0, 0.0)) <= Tolerance));
		}

		TEST_METHOD(DisplacementResistanceShortensTheDisplacement)
		{
			constexpr double Resistance = 0.5;
			FVeyraStatusSpec Anchor;
			Anchor.Id = FVeyraContentId::FromText(TEXT("anchor")).GetValue();
			Anchor.Kind = EVeyraStatusKind::DisplacementResistance;
			Anchor.Magnitude = Resistance;
			Anchor.DurationSeconds = 60.0;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Unit, *Unit, Anchor)));
			ASSERT_THAT(IsTrue(VeyraCombat::Displace(*Enemy, *Unit, KnockbackAlong(FVector::BackwardVector))));
			ASSERT_THAT(IsTrue(Movement->IsDisplaced() && Movement->IsMovementLocked()));
			const FVector Expected = Start - FVector(Distance * (1.0 - Resistance), 0.0, 0.0);
			ASSERT_THAT(IsTrue(FVector::Dist(Movement->GetForcedMoveDestination().GetValue(), Expected) <= Tolerance));
		}

		TEST_METHOD(ANewerDisplacementReplacesTheOlder)
		{
			ASSERT_THAT(IsTrue(VeyraCombat::Displace(*Enemy, *Unit, KnockbackAlong(FVector::BackwardVector))));
			ASSERT_THAT(IsTrue(VeyraCombat::Displace(*Enemy, *Unit, KnockbackAlong(FVector::RightVector))));
			ASSERT_THAT(IsTrue(FVector::Dist(Movement->GetForcedMoveDestination().GetValue(), Start + FVector(0.0, Distance, 0.0)) <= Tolerance));
		}

		TEST_METHOD(ADisplacementInterruptsADashAndTheUnit)
		{
			TArray<EVeyraDashEndReason> DashEnds;
			Movement->OnDashEnded.AddLambda([&DashEnds](const FVeyraDashEnd& End) { DashEnds.Add(End.Reason); });
			int32 Interruptions = 0;
			Unit->GetOwner()->FindComponentByClass<UVeyraStatusComponent>()->OnInterrupted.AddLambda([&Interruptions] { ++Interruptions; });

			ASSERT_THAT(IsTrue(VeyraCombat::Dash(*Unit, FVeyraDash{ FVector::BackwardVector, Distance, Speed, EVeyraDashContact::None })));
			ASSERT_THAT(IsTrue(Movement->IsDashing() && Movement->IsMovementLocked()));
			ASSERT_THAT(IsTrue(VeyraCombat::Displace(*Enemy, *Unit, KnockbackAlong(FVector::RightVector))));
			ASSERT_THAT(IsTrue(Movement->IsDisplaced()));
			ASSERT_THAT(IsTrue(DashEnds == TArray<EVeyraDashEndReason>{ EVeyraDashEndReason::Interrupted }));
			ASSERT_THAT(AreEqual(1, Interruptions));
		}

		TEST_METHOD(NoDashWhileDisplacedOrStunned)
		{
			const FVeyraDash Dash{ FVector::BackwardVector, Distance, Speed, EVeyraDashContact::None };
			ASSERT_THAT(IsTrue(VeyraCombat::Displace(*Enemy, *Unit, KnockbackAlong(FVector::RightVector))));
			ASSERT_THAT(IsFalse(VeyraCombat::Dash(*Unit, Dash)));

			FVeyraStatusSpec Daze;
			Daze.Id = FVeyraContentId::FromText(TEXT("daze")).GetValue();
			Daze.Kind = EVeyraStatusKind::Stun;
			Daze.DurationSeconds = 60.0;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Enemy, *Unit, Daze)));
			ASSERT_THAT(IsTrue(VeyraCombat::Displace(*Enemy, *Unit, KnockbackAlong(FVector::BackwardVector)), TEXT("a stunned unit can still be displaced")));
			ASSERT_THAT(IsFalse(VeyraCombat::Dash(*Unit, Dash)));
		}

		TEST_METHOD(RefusesForcedMovesOutOfRange)
		{
			TestRunner->AddExpectedMessagePlain(TEXT("Refused a displacement"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 3);
			TestRunner->AddExpectedMessagePlain(TEXT("Refused a dash"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1);
			ASSERT_THAT(IsFalse(VeyraCombat::Displace(*Enemy, *Unit, FVeyraDisplacement{ FVector::UpVector, Distance, Speed })));
			ASSERT_THAT(IsFalse(VeyraCombat::Displace(*Enemy, *Unit, FVeyraDisplacement{ FVector::ForwardVector, 0.0, Speed })));
			ASSERT_THAT(IsFalse(VeyraCombat::Displace(*Enemy, *Unit, FVeyraDisplacement{ FVector::ForwardVector, Distance, -Speed })));
			ASSERT_THAT(IsFalse(VeyraCombat::Dash(*Unit, FVeyraDash{ FVector::ForwardVector, Distance, 0.0, EVeyraDashContact::None })));
			ASSERT_THAT(IsFalse(Movement->IsMovementLocked()));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
