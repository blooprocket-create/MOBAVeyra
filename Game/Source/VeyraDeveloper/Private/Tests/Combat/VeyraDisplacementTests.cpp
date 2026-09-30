// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CQTest.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Greybox/VeyraGreyboxLayout.h"
#include "Life/VeyraCombatEventSubsystem.h"
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
			TArray<FVeyraDisplacementEvent> Displaced;
			Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>()->OnDisplaced.AddLambda(
				[&Displaced](const FVeyraDisplacementEvent& Event) { Displaced.Add(Event); });
			ASSERT_THAT(IsTrue(VeyraCombat::Displace(*Enemy, *Unit, KnockbackAlong(FVector::BackwardVector))));
			ASSERT_THAT(IsTrue(Movement->IsDisplaced() && Movement->IsMovementLocked()));
			const FVector Expected = Start - FVector(Distance * (1.0 - Resistance), 0.0, 0.0);
			ASSERT_THAT(IsTrue(FVector::Dist(Movement->GetForcedMoveDestination().GetValue(), Expected) <= Tolerance));
			// Who moved whom, and how far after resistance (ADR-018 §3).
			ASSERT_THAT(IsTrue(Displaced.Num() == 1 && Displaced[0].Source.Get() == Enemy && Displaced[0].Target.Get() == Unit));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Displaced[0].Distance, Distance * (1.0 - Resistance), Tolerance)));
		}

		TEST_METHOD(ItReportsTheDistanceTerrainLetsTheUnitGo)
		{
			TArray<FVeyraDisplacementEvent> Displaced;
			Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>()->OnDisplaced.AddLambda(
				[&Displaced](const FVeyraDisplacementEvent& Event) { Displaced.Add(Event); });
			// Toward the wall: it stops where the body meets it, short of the whole distance.
			ASSERT_THAT(IsTrue(VeyraCombat::Displace(*Enemy, *Unit, KnockbackAlong(FVector::ForwardVector))));
			const double Room = WallFaceX - Radius() - Start.X;
			ASSERT_THAT(IsTrue(Displaced.Num() == 1 && FMath::IsNearlyEqual(Displaced[0].Distance, Room, Tolerance),
				FString::Printf(TEXT("reported %g of %g"), Displaced.IsEmpty() ? 0.0 : Displaced[0].Distance, Room)));
			// Against the wall already, it cannot move at all: no displacement to report.
			AActor& Body = *Movement->GetOwner();
			Body.SetActorLocation(FVector(WallFaceX - Radius(), Start.Y, Start.Z));
			VeyraCombat::Displace(*Enemy, *Unit, KnockbackAlong(FVector::ForwardVector));
			ASSERT_THAT(IsTrue(Displaced.Num() == 1, TEXT("a unit that cannot move is not displaced")));
		}

		TEST_METHOD(TheUnstoppableAndTheImmuneStayPut)
		{
			for (const EVeyraStatusKind Kind : { EVeyraStatusKind::Unstoppable, EVeyraStatusKind::DisplacementImmunity })
			{
				FVeyraStatusSpec Guard;
				Guard.Id = FVeyraContentId::FromText(TEXT("guard")).GetValue();
				Guard.Kind = Kind;
				Guard.DurationSeconds = 60.0;
				ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Unit, *Unit, Guard)));
				ASSERT_THAT(IsFalse(VeyraCombat::Displace(*Enemy, *Unit, KnockbackAlong(FVector::BackwardVector))));
				ASSERT_THAT(IsFalse(Movement->IsDisplaced()));
				ASSERT_THAT(IsTrue(VeyraCombat::RemoveStatus(*Unit, Guard.Id)));
			}
		}

		TEST_METHOD(AFearedUnitFleesItsSourceUntilADisplacementTakesOver)
		{
			constexpr double Slowed = 0.5;
			constexpr double FearSeconds = 1.0;
			FVeyraStatusSpec Fear;
			Fear.Id = FVeyraContentId::FromText(TEXT("fear")).GetValue();
			Fear.Kind = EVeyraStatusKind::Fear;
			Fear.Magnitude = Slowed;
			Fear.DurationSeconds = FearSeconds;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Enemy, *Unit, Fear)));
			ASSERT_THAT(IsTrue(Movement->IsFleeing() && Movement->IsMovementLocked()));
			// Its source has no body here, so it flees the way it faces: toward the wall, never through it.
			const FVector End = Movement->GetForcedMoveDestination().GetValue();
			ASSERT_THAT(IsTrue(End.X > Start.X && End.X <= WallFaceX - Radius() + Tolerance, FString::Printf(TEXT("fled to X %g"), End.X)));
			ASSERT_THAT(IsTrue(VeyraCombat::Displace(*Enemy, *Unit, KnockbackAlong(FVector::BackwardVector))));
			ASSERT_THAT(IsTrue(Movement->IsDisplaced() && !Movement->IsFleeing()));
			ASSERT_THAT(IsFalse(Movement->StartFleeing(FVector::ForwardVector, Distance, Speed), TEXT("a Fear never replaces a displacement")));
		}

		TEST_METHOD(AFearEndedEarlyEndsTheFlight)
		{
			FVeyraStatusSpec Fear;
			Fear.Id = FVeyraContentId::FromText(TEXT("fear")).GetValue();
			Fear.Kind = EVeyraStatusKind::Fear;
			Fear.DurationSeconds = 1.0;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Enemy, *Unit, Fear) && Movement->IsFleeing()));
			ASSERT_THAT(IsTrue(VeyraCombat::RemoveStatus(*Unit, Fear.Id)));
			ASSERT_THAT(IsFalse(Movement->IsFleeing()));
			ASSERT_THAT(IsFalse(Movement->IsMovementLocked()));
		}

		TEST_METHOD(GhostedPassesThroughUnitsAndBodyScaleWidensTheBody)
		{
			UCapsuleComponent* Capsule = CastChecked<ACharacter>(Movement->GetOwner())->GetCapsuleComponent();
			const ECollisionResponse Before = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
			const float Width = Capsule->GetUnscaledCapsuleRadius();
			FVeyraStatusSpec Ghost;
			Ghost.Id = FVeyraContentId::FromText(TEXT("ghost")).GetValue();
			Ghost.Kind = EVeyraStatusKind::Ghosted;
			Ghost.DurationSeconds = 60.0;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Unit, *Unit, Ghost)));
			ASSERT_THAT(IsTrue(Capsule->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Ignore));
			ASSERT_THAT(IsTrue(VeyraCombat::RemoveStatus(*Unit, Ghost.Id) && Capsule->GetCollisionResponseToChannel(ECC_Pawn) == Before));

			constexpr double Scale = 1.5;
			FVeyraStatusSpec Big;
			Big.Id = FVeyraContentId::FromText(TEXT("big")).GetValue();
			Big.Kind = EVeyraStatusKind::BodyScale;
			Big.Magnitude = Scale;
			Big.DurationSeconds = 60.0;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Unit, *Unit, Big)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Capsule->GetUnscaledCapsuleRadius(), Width * Scale, 0.01)));
			ASSERT_THAT(IsTrue(VeyraCombat::RemoveStatus(*Unit, Big.Id) && FMath::IsNearlyEqual(Capsule->GetUnscaledCapsuleRadius(), Width, 0.01)));
		}

		TEST_METHOD(AStatusLeavesTheBodyItsCharacterSized)
		{
			// A Vanguard's character sizes its body from its data after its movement binds; a status
			// that shapes no body must leave that size, and the collision, as they are.
			UCapsuleComponent* Capsule = CastChecked<ACharacter>(Movement->GetOwner())->GetCapsuleComponent();
			const float Sized = Capsule->GetUnscaledCapsuleRadius() * 1.5f;
			Capsule->SetCapsuleRadius(Sized);
			const ECollisionResponse Response = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
			FVeyraStatusSpec Daze;
			Daze.Id = FVeyraContentId::FromText(TEXT("daze")).GetValue();
			Daze.Kind = EVeyraStatusKind::Stun;
			Daze.DurationSeconds = 60.0;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Enemy, *Unit, Daze)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Capsule->GetUnscaledCapsuleRadius(), Sized), FString::Printf(TEXT("radius %g"), Capsule->GetUnscaledCapsuleRadius())));
			ASSERT_THAT(IsTrue(VeyraCombat::RemoveStatus(*Unit, Daze.Id)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Capsule->GetUnscaledCapsuleRadius(), Sized) && Capsule->GetCollisionResponseToChannel(ECC_Pawn) == Response));
		}

		/** Moves any forced move on to its end, in steps, as its physics would. */
		void Settle() const
		{
			// Fixture values: a step, and more steps than any displacement here needs.
			constexpr float StepSeconds = 0.05f;
			constexpr int32 MaxSteps = 200;
			for (int32 Step = 0; Step < MaxSteps && Movement->IsDisplaced(); ++Step)
			{
				Movement->AdvanceForcedMove(StepSeconds);
			}
		}

		bool Stunned() const
		{
			return Unit->GetOwner()->FindComponentByClass<UVeyraStatusComponent>()->Has(EVeyraStatusKind::Stun);
		}

		static FVeyraDisplacement Colliding(const FVector& Direction)
		{
			FVeyraStatusSpec Stun;
			Stun.Id = FVeyraContentId::FromText(TEXT("collision_stun")).GetValue();
			Stun.Kind = EVeyraStatusKind::Stun;
			Stun.DurationSeconds = 60.0;
			FVeyraDisplacement Out = KnockbackAlong(Direction);
			Out.CollisionStatuses = { Stun };
			return Out;
		}

		TEST_METHOD(AKnockbackIntoTerrainStunsWithItsCollisionStatuses)
		{
			ASSERT_THAT(IsTrue(VeyraCombat::Displace(*Enemy, *Unit, Colliding(FVector::ForwardVector))));
			Settle();
			ASSERT_THAT(IsTrue(!Movement->IsDisplaced() && Stunned(), TEXT("the wall stops it, and it is stunned (ADR-028 §3)")));
		}

		TEST_METHOD(AKnockbackIntoAVanguardStopsThereAndStuns)
		{
			UAbilitySystemComponent& Other = SpawnCombatant(Spawner);
			ASSERT_THAT(IsTrue(VeyraCombat::InitializeStats(Other, ExampleStats())));
			AVeyraVanguardCharacter& Blocker = Spawner.SpawnActorAt<AVeyraVanguardCharacter>(FVector(-Distance / 2.0, 0.0, Start.Z), FRotator::ZeroRotator);
			Blocker.SetPlayerState(CastChecked<APlayerState>(Other.GetOwner()));
			ASSERT_THAT(IsTrue(VeyraCombat::Displace(*Enemy, *Unit, Colliding(FVector::BackwardVector))));
			Settle();
			const double Reached = Movement->GetOwner()->GetActorLocation().X;
			ASSERT_THAT(IsTrue(Reached > -Distance / 2.0 && Stunned(), *FString::Printf(TEXT("it stops at the Vanguard behind it: X %g"), Reached)));
		}

		TEST_METHOD(AKnockbackOverOpenGroundCollidesWithNothing)
		{
			ASSERT_THAT(IsTrue(VeyraCombat::Displace(*Enemy, *Unit, Colliding(FVector::BackwardVector))));
			Settle();
			ASSERT_THAT(IsTrue(FVector::Dist(Movement->GetOwner()->GetActorLocation(), Start - FVector(Distance, 0.0, 0.0)) <= Tolerance && !Stunned()));
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

		TEST_METHOD(ABlinkCrossesTerrainAtOnceAndEndsADash)
		{
			TArray<EVeyraDashEndReason> DashEnds;
			Movement->OnDashEnded.AddLambda([&DashEnds](const FVeyraDashEnd& End) { DashEnds.Add(End.Reason); });
			ASSERT_THAT(IsTrue(VeyraCombat::Dash(*Unit, FVeyraDash{ FVector::BackwardVector, Distance, Speed, EVeyraDashContact::None })));
			// Beyond the wall a dash would stop at (Combat Bible §9).
			const FVector Beyond(WallFaceX + WallThickness + Distance, 0.0, Start.Z);
			ASSERT_THAT(IsTrue(VeyraCombat::Blink(*Unit, Beyond, FVector::BackwardVector)));
			const AActor& Body = *Movement->GetOwner();
			ASSERT_THAT(IsTrue(FVector::Dist2D(Body.GetActorLocation(), Beyond) <= Tolerance,
				FString::Printf(TEXT("at once, past the wall: %s"), *Body.GetActorLocation().ToString())));
			ASSERT_THAT(IsTrue(Body.GetActorForwardVector().Equals(FVector::BackwardVector, KINDA_SMALL_NUMBER), TEXT("facing as asked")));
			ASSERT_THAT(IsTrue(!Movement->IsDashing() && DashEnds == TArray<EVeyraDashEndReason>{ EVeyraDashEndReason::Interrupted }));
		}

		TEST_METHOD(RootGroundedAndDisplacementRefuseABlink)
		{
			const FVector Aside(0.0, Distance, Start.Z);
			for (const EVeyraStatusKind Kind : { EVeyraStatusKind::Root, EVeyraStatusKind::Grounded })
			{
				FVeyraStatusSpec Held;
				Held.Id = FVeyraContentId::FromText(Kind == EVeyraStatusKind::Root ? TEXT("test_rooted") : TEXT("test_grounded")).GetValue();
				Held.Kind = Kind;
				Held.DurationSeconds = 60.0;
				ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Enemy, *Unit, Held)));
				ASSERT_THAT(IsFalse(VeyraCombat::Blink(*Unit, Aside), *UEnum::GetValueAsString(Kind)));
				VeyraCombat::RemoveStatus(*Unit, Held.Id);
			}
			ASSERT_THAT(IsTrue(VeyraCombat::Displace(*Enemy, *Unit, KnockbackAlong(FVector::BackwardVector))));
			ASSERT_THAT(IsFalse(VeyraCombat::Blink(*Unit, Aside), TEXT("nor while displaced")));
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
