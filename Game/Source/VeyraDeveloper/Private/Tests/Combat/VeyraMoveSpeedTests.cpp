// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Movement/VeyraMovementComponent.h"
#include "Movement/VeyraMovementRules.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tests/Combat/VeyraCombatTestHelpers.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "VeyraVanguardCharacter.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCombatTests
{
	/** Soft caps at 400 and 500 and a floor of 100, so the rules are checked apart from the committed tuning. Test fixture values. */
	inline FVeyraMovementTuning TestMovementTuning()
	{
		FVeyraMovementTuning Tuning;
		Tuning.SoftCaps = { { 400.0, 0.8 }, { 500.0, 0.5 } };
		Tuning.SlowFloor = 100.0;
		return Tuning;
	}

	inline FVeyraSpeedInputs SpeedInputs(double MoveSpeed, double StrongestSlow = 0.0)
	{
		FVeyraSpeedInputs Inputs;
		Inputs.MoveSpeed = MoveSpeed;
		Inputs.BaseMoveSpeed = MoveSpeed;
		Inputs.StrongestSlow = StrongestSlow;
		return Inputs;
	}

	// Veyra.Combat.MoveSpeed.*: effective Movement Speed (Combat Bible §23, §39; ADR-009 §2), and the
	// movement component that walks at it.
	TEST_CLASS(MoveSpeed, "Veyra.Combat")
	{
		const FVeyraMovementTuning Tuning = TestMovementTuning();
		FActorTestSpawner Spawner;

		double Speed(const FVeyraSpeedInputs& Inputs) const
		{
			return VeyraMovementRules::EffectiveSpeed(Inputs, Tuning);
		}

		TEST_METHOD(SpeedBelowTheFirstCapIsKeptInFull)
		{
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Speed(SpeedInputs(300.0)), 300.0)));
		}

		TEST_METHOD(SoftCapsKeepPartOfEachExtraUnit)
		{
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Speed(SpeedInputs(450.0)), 400.0 + 0.8 * 50.0)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Speed(SpeedInputs(600.0)), 400.0 + 0.8 * 100.0 + 0.5 * 100.0)));
		}

		TEST_METHOD(TheBonusAndTheSlowComeBeforeTheSoftCaps)
		{
			FVeyraSpeedInputs Inputs = SpeedInputs(500.0, 0.25);
			Inputs.ConditionalBonus = 0.2;
			const double BeforeCaps = 500.0 * 1.2 * 0.75;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Speed(Inputs), 400.0 + 0.8 * (BeforeCaps - 400.0))));
		}

		TEST_METHOD(SlowingStopsAtTheFloor)
		{
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Speed(SpeedInputs(300.0, 0.9)), Tuning.SlowFloor)));
		}

		TEST_METHOD(TheFloorNeverRaisesAUnitAboveItsBaseSpeed)
		{
			constexpr double SlowUnit = 80.0;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Speed(SpeedInputs(SlowUnit)), SlowUnit)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Speed(SpeedInputs(SlowUnit, 0.5)), SlowUnit)));
		}

		TEST_METHOD(AStunStopsMovement)
		{
			FVeyraSpeedInputs Inputs = SpeedInputs(300.0);
			Inputs.bStunned = true;
			ASSERT_THAT(IsTrue(Speed(Inputs) == 0.0));
		}

		TEST_METHOD(ValidationCatchesSoftCapsOutOfOrder)
		{
			FVeyraCombatTuning Broken = UVeyraCombatTuningSubsystem::Get();
			Broken.Movement.SoftCaps = { { 500.0, 0.5 }, { 400.0, 0.8 } };
			const TArray<FString> Problems = VeyraCombatTuningRules::Validate(Broken);
			ASSERT_THAT(IsTrue(Problems.ContainsByPredicate([](const FString& Problem) { return Problem.StartsWith(TEXT("/movement/softCaps/1/from:")); }),
				FString::Join(Problems, TEXT(" | "))));
		}

		TEST_METHOD(AVanguardWalksAtItsEffectiveSpeed)
		{
			constexpr double Slow = 0.3;
			constexpr double LongSeconds = 60.0;
			UAbilitySystemComponent& Unit = SpawnCombatant(Spawner);
			ASSERT_THAT(IsTrue(VeyraCombat::InitializeStats(Unit, ExampleStats())));
			AVeyraVanguardCharacter& Vanguard = Spawner.SpawnActor<AVeyraVanguardCharacter>();
			Vanguard.SetPlayerState(CastChecked<APlayerState>(Unit.GetOwner()));
			UVeyraMovementComponent& Movement = *Vanguard.GetVeyraMovement();
			Movement.SetMovementMode(MOVE_Walking);
			const FVeyraMovementTuning& Committed = UVeyraCombatTuningSubsystem::Get().Movement;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Movement.GetMaxSpeed(), VeyraMovementRules::EffectiveSpeed(SpeedInputs(ExampleStats().MoveSpeed), Committed), 1e-3)));

			FVeyraStatusSpec Chill;
			Chill.Id = FVeyraContentId::FromText(TEXT("chill")).GetValue();
			Chill.Kind = EVeyraStatusKind::Slow;
			Chill.Magnitude = Slow;
			Chill.DurationSeconds = LongSeconds;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Unit, Unit, Chill)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Movement.GetMaxSpeed(), VeyraMovementRules::EffectiveSpeed(SpeedInputs(ExampleStats().MoveSpeed, Slow), Committed), 1e-3)));

			TArray<bool> LockChanges;
			Movement.OnMovementLockChanged.AddLambda([&LockChanges](bool bLocked) { LockChanges.Add(bLocked); });
			FVeyraStatusSpec Daze = Chill;
			Daze.Id = FVeyraContentId::FromText(TEXT("daze")).GetValue();
			Daze.Kind = EVeyraStatusKind::Stun;
			Daze.Magnitude = 0.0;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Unit, Unit, Daze)));
			ASSERT_THAT(IsTrue(Movement.GetMaxSpeed() == 0.0f));
			ASSERT_THAT(IsTrue(Movement.IsMovementLocked()));
			ASSERT_THAT(IsTrue(VeyraCombat::RemoveStatus(Unit, Daze.Id)));
			ASSERT_THAT(IsFalse(Movement.IsMovementLocked()));
			ASSERT_THAT(IsTrue(LockChanges == TArray<bool>{ true, false }));
		}

		TEST_METHOD(SlowResistanceWeakensTheSlowAndPlantedStopsTheUnit)
		{
			constexpr double Slow = 0.4;
			constexpr double Resistance = 0.5;
			constexpr double LongSeconds = 60.0;
			UAbilitySystemComponent& Unit = SpawnCombatant(Spawner);
			ASSERT_THAT(IsTrue(VeyraCombat::InitializeStats(Unit, ExampleStats())));
			AVeyraVanguardCharacter& Vanguard = Spawner.SpawnActor<AVeyraVanguardCharacter>();
			Vanguard.SetPlayerState(CastChecked<APlayerState>(Unit.GetOwner()));
			UVeyraMovementComponent& Movement = *Vanguard.GetVeyraMovement();
			Movement.SetMovementMode(MOVE_Walking);
			const FVeyraMovementTuning& Committed = UVeyraCombatTuningSubsystem::Get().Movement;
			const auto Status = [LongSeconds](const TCHAR* Id, EVeyraStatusKind Kind, double Magnitude) {
				FVeyraStatusSpec Spec;
				Spec.Id = FVeyraContentId::FromText(Id).GetValue();
				Spec.Kind = Kind;
				Spec.Magnitude = Magnitude;
				Spec.DurationSeconds = LongSeconds;
				return Spec;
			};
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Unit, Unit, Status(TEXT("chill"), EVeyraStatusKind::Slow, Slow))));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Unit, Unit, Status(TEXT("steady"), EVeyraStatusKind::SlowResistance, Resistance))));
			// A 40% Slow against 50% Slow Resistance slows by 20% (ADR-018 §2).
			const double Expected = VeyraMovementRules::EffectiveSpeed(SpeedInputs(ExampleStats().MoveSpeed, Slow * (1.0 - Resistance)), Committed);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Movement.GetMaxSpeed(), Expected, 1e-3)));

			// A firing stance: the unit stands still by its own choice.
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Unit, Unit, Status(TEXT("dig_in"), EVeyraStatusKind::Planted, 0.0))));
			ASSERT_THAT(IsTrue(Movement.GetMaxSpeed() == 0.0f && Movement.IsMovementLocked()));
		}

		TEST_METHOD(HeadingTowardAUnitMeansItLiesWithinTheAngle)
		{
			// Fixture values: a unit heading along +X, and a 60-degree allowance either side.
			constexpr double MaxAngle = 60.0;
			const FVector Heading(1.0, 0.0, 0.0);
			ASSERT_THAT(IsTrue(VeyraMovementRules::IsHeadingToward(FVector::ZeroVector, Heading, FVector(100.0, 50.0, 0.0), MaxAngle)));
			ASSERT_THAT(IsFalse(VeyraMovementRules::IsHeadingToward(FVector::ZeroVector, Heading, FVector(0.0, 100.0, 0.0), MaxAngle), TEXT("beside it")));
			ASSERT_THAT(IsFalse(VeyraMovementRules::IsHeadingToward(FVector::ZeroVector, -Heading, FVector(100.0, 0.0, 0.0), MaxAngle), TEXT("behind it")));
			ASSERT_THAT(IsFalse(VeyraMovementRules::IsHeadingToward(FVector::ZeroVector, FVector::ZeroVector, FVector(100.0, 0.0, 0.0), MaxAngle), TEXT("standing still")));
		}

		TEST_METHOD(ABonusTowardEnemyVanguardsHoldsOnlyWhileHeadingForOne)
		{
			// Fixture values: the bonus, a pace, and how far inside the pursuit range the enemy stands.
			constexpr double Bonus = 0.2;
			constexpr double Pace = 300.0;
			constexpr double Inside = 100.0;
			constexpr double LongSeconds = 60.0;
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Pursuer = World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			const double Range = UVeyraCombatTuningSubsystem::Get().Pursuit.Range;
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Range - Inside, 0.0, 0.0));
			UAbilitySystemComponent& Unit = *Pursuer.GetAbilitySystemComponent();

			FVeyraStatusSpec Hunt;
			Hunt.Id = FVeyraContentId::FromText(TEXT("hunt")).GetValue();
			Hunt.Kind = EVeyraStatusKind::MoveSpeedTowardEnemyVanguards;
			Hunt.Magnitude = Bonus;
			Hunt.DurationSeconds = LongSeconds;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Unit, Unit, Hunt)));

			UVeyraMovementComponent& Movement = *Pursuer.GetVeyraMovement();
			const FVeyraMovementTuning& Committed = UVeyraCombatTuningSubsystem::Get().Movement;
			const double Plain = VeyraMovementRules::EffectiveSpeed(SpeedInputs(ExampleStats().MoveSpeed), Committed);
			FVeyraSpeedInputs Hunting = SpeedInputs(ExampleStats().MoveSpeed);
			Hunting.ConditionalBonus = Bonus;
			const double Faster = VeyraMovementRules::EffectiveSpeed(Hunting, Committed);

			Movement.Velocity = FVector(Pace, 0.0, 0.0);
			ASSERT_THAT(IsTrue(Movement.IsMovingTowardEnemyVanguard() && FMath::IsNearlyEqual(Movement.GetMaxSpeed(), Faster, 1e-3)));
			Movement.Velocity = FVector(-Pace, 0.0, 0.0);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Movement.GetMaxSpeed(), Plain, 1e-3), TEXT("moving away")));

			Movement.Velocity = FVector(Pace, 0.0, 0.0);
			Enemy.SetActorLocation(FVector(Range + Pursuer.GetSimpleCollisionRadius() + Enemy.GetSimpleCollisionRadius() + Inside, 0.0, 0.0));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Movement.GetMaxSpeed(), Plain, 1e-3), TEXT("out of range, edge to edge")));
			World.Spawn(EVeyraTeam::A, FVector(Range - Inside, 0.0, 0.0));
			ASSERT_THAT(IsFalse(Movement.IsMovingTowardEnemyVanguard(), TEXT("an ally ahead does not count")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
