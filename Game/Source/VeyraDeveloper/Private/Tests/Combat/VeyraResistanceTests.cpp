// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Damage/VeyraDamageResolver.h"
#include "Tuning/VeyraCombatTuning.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCombatTests
{
	// The constant Combat Bible §3 uses for its worked examples. The game reads its value from
	// Game/Tuning/Combat.json; these tests pass it explicitly.
	static constexpr double CanonMitigationConstant = 100.0;
	static constexpr double ResistanceTolerance = 1e-12;

	// Veyra.Combat.ResistanceMitigation.*: the Armor/Magic Resistance curve (Combat Bible §3).
	TEST_CLASS(ResistanceMitigation, "Veyra.Combat")
	{
		TEST_METHOD(MatchesTheCanonTable)
		{
			struct FRow
			{
				double Resistance;
				double DamageTaken;
			};
			const FRow Rows[] = {
				{ 0.0, 1.0 }, { 50.0, 2.0 / 3.0 }, { 100.0, 0.5 }, { 200.0, 1.0 / 3.0 }, { 300.0, 0.25 },
				{ -25.0, 1.2 }, { -50.0, 4.0 / 3.0 }, { -100.0, 1.5 }, { -200.0, 5.0 / 3.0 },
			};
			for (const FRow& Row : Rows)
			{
				const double Actual = VeyraDamage::ResistanceDamageMultiplier(Row.Resistance, CanonMitigationConstant);
				ASSERT_THAT(IsNear(Row.DamageTaken, Actual, ResistanceTolerance, FString::Printf(TEXT("Resistance %g"), Row.Resistance)));
			}
		}

		TEST_METHOD(IsExactlyOneAtZeroForAnyConstant)
		{
			for (const double Constant : { 1.0, 100.0, 250.5 })
			{
				ASSERT_THAT(IsTrue(VeyraDamage::ResistanceDamageMultiplier(0.0, Constant) == 1.0, FString::Printf(TEXT("K = %g"), Constant)));
			}
		}

		TEST_METHOD(IsContinuousAcrossZero)
		{
			const double JustBelow = VeyraDamage::ResistanceDamageMultiplier(-1e-9, CanonMitigationConstant);
			const double JustAbove = VeyraDamage::ResistanceDamageMultiplier(1e-9, CanonMitigationConstant);
			ASSERT_THAT(IsNear(JustBelow, JustAbove, 1e-10));
		}

		TEST_METHOD(NegativeResistanceNeverReachesDoubleDamage)
		{
			double Previous = 1.0;
			for (const double Resistance : { -1e3, -1e6, -1e9, -1e12 })
			{
				const double Multiplier = VeyraDamage::ResistanceDamageMultiplier(Resistance, CanonMitigationConstant);
				ASSERT_THAT(IsTrue(Multiplier < 2.0 && Multiplier > Previous, FString::Printf(TEXT("Resistance %g gave %.17g"), Resistance, Multiplier)));
				Previous = Multiplier;
			}
		}

		TEST_METHOD(ScalesWithTheTunedConstant)
		{
			// With K doubled, twice the resistance is needed for the same mitigation.
			ASSERT_THAT(IsNear(0.5, VeyraDamage::ResistanceDamageMultiplier(200.0, 2.0 * CanonMitigationConstant), ResistanceTolerance));
			ASSERT_THAT(IsNear(1.5, VeyraDamage::ResistanceDamageMultiplier(-200.0, 2.0 * CanonMitigationConstant), ResistanceTolerance));
		}

		TEST_METHOD(ArmorAndMagicResistStaySeparate)
		{
			FVeyraResistanceTuning Tuning;
			Tuning.MitigationConstant = CanonMitigationConstant;
			FVeyraDamagePayload Payload;
			Payload.Components = { { EVeyraDamageType::Physical, 100.0 }, { EVeyraDamageType::Magic, 100.0 } };
			FVeyraDefenderDefence Defence;
			Defence.Armor = 100.0;
			Defence.MagicResist = 0.0;

			const FVeyraMitigatedDamage Result = VeyraDamage::ApplyTargetSide(Payload, Defence, Tuning);
			ASSERT_THAT(IsNear(50.0, Result.Components[0].Amount, ResistanceTolerance, TEXT("Physical meets Armor")));
			ASSERT_THAT(IsNear(100.0, Result.Components[1].Amount, ResistanceTolerance, TEXT("Magic ignores Armor")));
		}
	};

	// Veyra.Combat.ResistanceOrder.*: reduction and penetration (Combat Bible §3, author rulings of
	// 2026-09-25 on percentage sources).
	TEST_CLASS(ResistanceOrder, "Veyra.Combat")
	{
		TEST_METHOD(FollowsTheCanonOrder)
		{
			// 100 − 20 flat reduction = 80; ×0.75 (25% reduction) = 60; ×0.6 (40% penetration) = 36;
			// − 10 flat penetration = 26. Any other order gives a different result.
			const FVeyraResistanceReduction Reduction{ 20.0, 0.75 };
			const FVeyraPenetration Penetration{ 10.0, 0.6 };
			ASSERT_THAT(IsNear(26.0, VeyraDamage::ResolveEffectiveResistance(100.0, Reduction, Penetration), ResistanceTolerance));
		}

		TEST_METHOD(FlatReductionCanPushResistanceBelowZero)
		{
			ASSERT_THAT(IsNear(-20.0, VeyraDamage::ReduceResistance(10.0, FVeyraResistanceReduction{ 30.0, 1.0 }), ResistanceTolerance));
		}

		TEST_METHOD(PercentageReductionDoesNothingAtOrBelowZero)
		{
			ASSERT_THAT(IsNear(-20.0, VeyraDamage::ReduceResistance(10.0, FVeyraResistanceReduction{ 30.0, 0.5 }), ResistanceTolerance));
			ASSERT_THAT(IsNear(0.0, VeyraDamage::ReduceResistance(0.0, FVeyraResistanceReduction{ 0.0, 0.5 }), ResistanceTolerance));
		}

		TEST_METHOD(PenetrationNeverPushesResistanceBelowZero)
		{
			ASSERT_THAT(IsNear(0.0, VeyraDamage::ResolveEffectiveResistance(30.0, FVeyraResistanceReduction(), FVeyraPenetration{ 50.0, 1.0 }), ResistanceTolerance));
		}

		TEST_METHOD(PenetrationDoesNothingToNegativeResistance)
		{
			const FVeyraResistanceReduction Reduction{ 40.0, 1.0 };
			ASSERT_THAT(IsNear(-20.0, VeyraDamage::ResolveEffectiveResistance(20.0, Reduction, FVeyraPenetration{ 10.0, 0.5 }), ResistanceTolerance));
		}

		TEST_METHOD(PercentageSourcesCombineMultiplicatively)
		{
			// Two 40% penetration sources keep 0.6 × 0.6 = 36% of the resistance (64% penetration).
			const FVeyraPenetration TwoSources{ 0.0, 0.6 * 0.6 };
			ASSERT_THAT(IsNear(36.0, VeyraDamage::ResolveEffectiveResistance(100.0, FVeyraResistanceReduction(), TwoSources), ResistanceTolerance));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
