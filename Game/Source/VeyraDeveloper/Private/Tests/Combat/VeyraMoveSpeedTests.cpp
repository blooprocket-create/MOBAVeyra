// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Movement/VeyraMovementComponent.h"
#include "Movement/VeyraMovementRules.h"
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
	};
}

#endif // WITH_AUTOMATION_WORKER
