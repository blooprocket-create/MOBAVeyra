// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Movement/VeyraMovementRules.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCombatTests
{
	using VeyraAbilitiesTests::FArchetypeTestWorld;

	// Veyra.Combat.Riding.*: a ride state's movement (Combat Bible §56): a set speed, a limited turn,
	// passing through units, no attacks, and a decay back to ordinary speed. What it does over real
	// frames is Veyra.Net.ForcedMovement's.
	TEST_CLASS(Riding, "Veyra.Combat")
	{
		// Fixture values: a fast ride with a slow turn, and its decay.
		static constexpr double RideSpeed = 800.0;
		static constexpr double TurnRate = 90.0;
		static constexpr double DecaySeconds = 2.0;
		static constexpr double Slow = 0.25;
		static constexpr float Frame = 0.1f;
		static constexpr double Tolerance = 1.0;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Rider = nullptr;

		BEFORE_EACH()
		{
			FArchetypeTestWorld World{ Spawner };
			Rider = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
		}

		UVeyraMovementComponent& Movement() const
		{
			return *Rider->GetVeyraMovement();
		}

		UAbilitySystemComponent& Abilities() const
		{
			return *Rider->GetAbilitySystemComponent();
		}

		bool Mount(double Decay = 0.0) const
		{
			return VeyraCombat::StartRide(Abilities(), FVeyraRide{ RideSpeed, TurnRate, Decay });
		}

		void Wait(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, Frame);
			}
		}

		TEST_METHOD(ASetSpeedIgnoresTheSoftCapsButNotSlowsOrTheFloor)
		{
			const FVeyraMovementTuning& Tuning = UVeyraCombatTuningSubsystem::Get().Movement;
			FVeyraSpeedInputs Inputs;
			Inputs.MoveSpeed = VeyraCombatTests::ExampleStats().MoveSpeed;
			Inputs.BaseMoveSpeed = Inputs.MoveSpeed;
			Inputs.SetSpeed = RideSpeed;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraMovementRules::EffectiveSpeed(Inputs, Tuning), RideSpeed, Tolerance), TEXT("no soft cap")));
			Inputs.StrongestSlow = Slow;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraMovementRules::EffectiveSpeed(Inputs, Tuning), RideSpeed * (1.0 - Slow), Tolerance)));
			Inputs.StrongestSlow = 0.99;
			ASSERT_THAT(IsTrue(VeyraMovementRules::EffectiveSpeed(Inputs, Tuning) >= FMath::Min(Tuning.SlowFloor, Inputs.BaseMoveSpeed) - Tolerance, TEXT("the floor holds")));
		}

		TEST_METHOD(ARiderPassesThroughUnitsMayCastAndCannotAttack)
		{
			ASSERT_THAT(IsTrue(Mount() && Movement().IsRiding()));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Movement().GetMaxSpeed(), RideSpeed, Tolerance)));
			ASSERT_THAT(IsTrue(Rider->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Ignore));
			const EVeyraActionBlocks Blocks = VeyraCombat::GetActionBlocks(Abilities());
			ASSERT_THAT(IsTrue(EnumHasAnyFlags(Blocks, EVeyraActionBlocks::Attack) && !EnumHasAnyFlags(Blocks, EVeyraActionBlocks::Cast | EVeyraActionBlocks::Move)));
		}

		TEST_METHOD(ItsHeadingTurnsNoFasterThanItsRate)
		{
			ASSERT_THAT(IsTrue(Mount()));
			// Heading +X, it is asked to go straight back.
			Movement().Velocity = FVector::ForwardVector * RideSpeed;
			Movement().RequestDirectMove(FVector::BackwardVector * RideSpeed, /*bForceMaxSpeed*/ true);
			Movement().CalcVelocity(Frame, Movement().GroundFriction, false, Movement().GetMaxBrakingDeceleration());
			const double Turned = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Movement().Velocity.GetSafeNormal2D(), FVector::ForwardVector)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Turned, TurnRate * Frame, 0.5), FString::Printf(TEXT("turned %g degrees"), Turned)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Movement().GetRideTurnRadius(), RideSpeed / FMath::DegreesToRadians(TurnRate), Tolerance)));
		}

		TEST_METHOD(NoCrowdControlEndsARide)
		{
			ASSERT_THAT(IsTrue(Mount()));
			FVeyraStatusSpec Daze;
			Daze.Id = FVeyraContentId::FromText(TEXT("daze")).GetValue();
			Daze.Kind = EVeyraStatusKind::Stun;
			Daze.DurationSeconds = 60.0;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Abilities(), Abilities(), Daze)));
			ASSERT_THAT(IsTrue(Movement().IsRiding() && Movement().GetMaxSpeed() == 0.0, TEXT("stunned, it stays mounted and still")));
			ASSERT_THAT(IsTrue(VeyraCombat::RemoveStatus(Abilities(), Daze.Id)));
			ASSERT_THAT(IsTrue(VeyraCombat::Displace(Abilities(), Abilities(), FVeyraDisplacement{ FVector::RightVector, 200.0, 1000.0 })));
			ASSERT_THAT(IsTrue(Movement().IsRiding(), TEXT("displaced, it stays mounted")));
		}

		TEST_METHOD(LeavingTheRideItSlowsToItsOrdinarySpeedAcrossItsDecay)
		{
			TArray<FVeyraRideEnd> Ends;
			Movement().OnRideEnded.AddLambda([&Ends](const FVeyraRideEnd& End) { Ends.Add(End); });
			ASSERT_THAT(IsTrue(Mount(DecaySeconds)));
			VeyraCombat::EndRide(Abilities(), EVeyraRideEndReason::Dismounted);
			ASSERT_THAT(IsTrue(Ends.Num() == 1 && Ends[0].Reason == EVeyraRideEndReason::Dismounted && !Movement().IsRiding()));
			const double Ordinary = VeyraCombatTests::ExampleStats().MoveSpeed;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Movement().GetMaxSpeed(), RideSpeed, Tolerance), TEXT("it keeps its speed at first")));
			Wait(DecaySeconds / 2.0);
			const double Halfway = Movement().GetMaxSpeed();
			ASSERT_THAT(IsTrue(Halfway < RideSpeed - Tolerance && Halfway > Ordinary + Tolerance, FString::Printf(TEXT("%g halfway"), Halfway)));
			Wait(DecaySeconds / 2.0 + Frame);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Movement().GetMaxSpeed(), Ordinary, Tolerance)));
			ASSERT_THAT(IsTrue(Rider->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Ignore, TEXT("it blocks units again")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
