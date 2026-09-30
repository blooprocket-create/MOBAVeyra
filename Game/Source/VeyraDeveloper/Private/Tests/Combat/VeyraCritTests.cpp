// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attacks/VeyraCrit.h"
#include "CQTest.h"
#include "Tuning/VeyraCombatTuning.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCritTests
{
	// Veyra.Combat.CritRule.*: the critical-strike rule (Combat Bible §5; ADR-023 §1).
	TEST_CLASS(CritRule, "Veyra.Combat")
	{
		// Fixture values: Combat Bible §5's prototype baseline, restated so the examples below hold.
		static FVeyraCritTuning Canon()
		{
			FVeyraCritTuning Tuning;
			Tuning.Damage = 1.75;
			Tuning.ChanceCap = 1.0;
			Tuning.OverflowDamagePerChance = 0.5;
			return Tuning;
		}

		TEST_METHOD(ItCritsWhenTheRollIsBelowTheChance)
		{
			ASSERT_THAT(IsTrue(VeyraCrit::Resolve(0.3, 0.0, Canon(), 0.29).bCritical));
			ASSERT_THAT(IsFalse(VeyraCrit::Resolve(0.3, 0.0, Canon(), 0.3).bCritical));
			ASSERT_THAT(IsFalse(VeyraCrit::Resolve(0.0, 0.0, Canon(), 0.0).bCritical, TEXT("no chance, no crit")));
			ASSERT_THAT(IsTrue(VeyraCrit::Resolve(1.0, 0.0, Canon(), 0.9999).bCritical, TEXT("a full chance always crits")));
			ASSERT_THAT(IsFalse(VeyraCrit::Resolve(-0.5, 0.0, Canon(), 0.0).bCritical, TEXT("a negative chance counts as none")));
		}

		TEST_METHOD(ChanceAboveTheCapBecomesCritDamage)
		{
			// Combat Bible §5's own examples.
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraCrit::Resolve(0.5, 0.0, Canon(), 0.9).Multiplier, 1.75)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraCrit::Resolve(1.2, 0.0, Canon(), 0.9).Multiplier, 1.85)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraCrit::Resolve(1.5, 0.0, Canon(), 0.9).Multiplier, 2.0)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraCrit::Resolve(2.0, 0.0, Canon(), 0.9).Multiplier, 2.25)));
			ASSERT_THAT(IsTrue(VeyraCrit::Resolve(1.2, 0.0, Canon(), 0.9999).bCritical, TEXT("the effective chance is capped at 100%")));
		}

		TEST_METHOD(ABonusAddsToTheCritDamage)
		{
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraCrit::Resolve(0.5, 0.4, Canon(), 0.1).Multiplier, 2.15)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraCrit::Resolve(1.2, 0.4, Canon(), 0.1).Multiplier, 2.25), TEXT("bonus and overflow both add")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraCrit::Resolve(0.5, -1.0, Canon(), 0.1).Multiplier, 1.75), TEXT("a negative bonus counts as none")));
		}

		TEST_METHOD(TheShippedValuesAreTheBiblesBaseline)
		{
			const FVeyraCritTuning& Shipped = UVeyraCombatTuningSubsystem::Get().Crit;
			ASSERT_THAT(IsTrue(Shipped.Provenance == EVeyraTuningProvenance::Canon));
			ASSERT_THAT(IsTrue(Shipped.Damage == Canon().Damage && Shipped.ChanceCap == Canon().ChanceCap
				&& Shipped.OverflowDamagePerChance == Canon().OverflowDamagePerChance));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
