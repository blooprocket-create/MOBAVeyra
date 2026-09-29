// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attacks/VeyraAttackSpeed.h"
#include "CQTest.h"
#include "Tuning/VeyraCombatTuning.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCombatTests
{
	// Veyra.Combat.AttackSpeed.*: attacks per second between the minimum and the cap, a kit's own
	// interval floor, and overflow damage measured from the cap (Combat Bible §22, §39; ADR-009 §5).
	TEST_CLASS(AttackSpeed, "Veyra.Combat")
	{
		static constexpr double Tolerance = 1e-9;

		/** The formula's values as Combat Bible §22 prints them, so its examples can be checked. Fixture values. */
		static FVeyraAttackSpeedTuning CanonFormula()
		{
			FVeyraAttackSpeedTuning Tuning;
			Tuning.Cap = 2.5;
			Tuning.Minimum = 0.2;
			Tuning.OverflowDamageScalePercent = 132.0;
			Tuning.OverflowCurveConstant = 200.0;
			return Tuning;
		}

		TEST_METHOD(AttacksStayBetweenTheMinimumAndTheCap)
		{
			const FVeyraAttackSpeedTuning& Tuning = UVeyraCombatTuningSubsystem::Get().AttackSpeed;
			const FVeyraAttackTiming Fast = VeyraAttackSpeed::Resolve(Tuning.Cap * 3.0, Tuning, 0.0);
			ASSERT_THAT(IsTrue(Fast.AttacksPerSecond == Tuning.Cap && FMath::IsNearlyEqual(Fast.IntervalSeconds, 1.0 / Tuning.Cap, Tolerance)));
			const FVeyraAttackTiming Slow = VeyraAttackSpeed::Resolve(Tuning.Minimum / 2.0, Tuning, 0.0);
			ASSERT_THAT(IsTrue(Slow.AttacksPerSecond == Tuning.Minimum && Slow.OverflowDamageMultiplier == 1.0));
			const double Between = (Tuning.Minimum + Tuning.Cap) / 2.0;
			ASSERT_THAT(IsTrue(VeyraAttackSpeed::Resolve(Between, Tuning, 0.0).AttacksPerSecond == Between));
		}

		TEST_METHOD(OverflowMatchesTheCanonExamples)
		{
			// Combat Bible §22: overflow of 10%, 50%, 100% and 200% of the cap adds ~6.3%, 26.4%, 44% and 66%.
			const FVeyraAttackSpeedTuning Tuning = CanonFormula();
			struct FExample
			{
				double Overflow;
				double Bonus;
				double Allowance;
			};
			const FExample Examples[] = { { 0.1, 0.063, 1e-3 }, { 0.5, 0.264, Tolerance }, { 1.0, 0.44, Tolerance }, { 2.0, 0.66, Tolerance } };
			for (const FExample& Example : Examples)
			{
				const FVeyraAttackTiming Timing = VeyraAttackSpeed::Resolve(Tuning.Cap * (1.0 + Example.Overflow), Tuning, 0.0);
				ASSERT_THAT(IsTrue(Timing.AttacksPerSecond == Tuning.Cap, TEXT("overflow never raises the cap")));
				ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Timing.OverflowDamageMultiplier, 1.0 + Example.Bonus, Example.Allowance),
					FString::Printf(TEXT("%g overflow gave %g"), Example.Overflow, Timing.OverflowDamageMultiplier)));
			}
		}

		TEST_METHOD(APersonalFloorSlowsAttacksButMakesNoOverflow)
		{
			const FVeyraAttackSpeedTuning& Tuning = UVeyraCombatTuningSubsystem::Get().AttackSpeed;
			// Fixture value: a floor longer than the interval at the cap.
			const double FloorSeconds = 2.0 / Tuning.Cap;
			const FVeyraAttackTiming AtCap = VeyraAttackSpeed::Resolve(Tuning.Cap, Tuning, FloorSeconds);
			ASSERT_THAT(IsTrue(AtCap.IntervalSeconds == FloorSeconds));
			ASSERT_THAT(IsTrue(AtCap.OverflowDamageMultiplier == 1.0, TEXT("reaching the floor is not overflow")));
			const FVeyraAttackTiming Beyond = VeyraAttackSpeed::Resolve(Tuning.Cap * 2.0, Tuning, FloorSeconds);
			ASSERT_THAT(IsTrue(Beyond.IntervalSeconds == FloorSeconds && Beyond.OverflowDamageMultiplier > 1.0, TEXT("past the cap it still overflows")));
		}

		TEST_METHOD(ARaisedCapAttacksFasterButOverflowStillCountsFromTheOrdinaryCap)
		{
			const FVeyraAttackSpeedTuning& Tuning = UVeyraCombatTuningSubsystem::Get().AttackSpeed;
			const double Uncapped = Tuning.Cap * 1.4;
			const double Raised = Tuning.Cap * 1.2;
			const FVeyraAttackTiming Ordinary = VeyraAttackSpeed::Resolve(Uncapped, Tuning, 0.0);
			const FVeyraAttackTiming Lifted = VeyraAttackSpeed::Resolve(Uncapped, Tuning, 0.0, Raised);
			ASSERT_THAT(IsTrue(Ordinary.AttacksPerSecond == Tuning.Cap && FMath::IsNearlyEqual(Lifted.AttacksPerSecond, Raised)));
			// §22: the ordinary cap stays the reference, so the overflow damage is the same.
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Lifted.OverflowDamageMultiplier, Ordinary.OverflowDamageMultiplier)));
			// A "raised" cap below the ordinary one changes nothing.
			ASSERT_THAT(IsTrue(VeyraAttackSpeed::Resolve(Uncapped, Tuning, 0.0, Tuning.Cap / 2.0).AttacksPerSecond == Tuning.Cap));
		}

		TEST_METHOD(ValidationKeepsTheMinimumBelowTheCap)
		{
			FVeyraCombatTuning Tuning = UVeyraCombatTuningSubsystem::Get();
			ASSERT_THAT(IsTrue(VeyraCombatTuningRules::Validate(Tuning).IsEmpty()));
			Tuning.AttackSpeed.Minimum = Tuning.AttackSpeed.Cap;
			ASSERT_THAT(IsTrue(VeyraCombatTuningRules::Validate(Tuning).ContainsByPredicate(
				[](const FString& Problem) { return Problem.StartsWith(TEXT("/attackSpeed/minimum:")); })));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
