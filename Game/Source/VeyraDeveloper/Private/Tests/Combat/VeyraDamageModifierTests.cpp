// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Damage/VeyraDamageResolver.h"
#include "Tuning/VeyraCombatTuning.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCombatTests
{
	// Veyra.Combat.DamageModifiers.*: generic amplification and reduction (Combat Bible §15) and the
	// component rules of §25.
	TEST_CLASS(DamageModifiers, "Veyra.Combat")
	{
		static constexpr double Tolerance = 1e-12;

		FVeyraResistanceTuning Tuning;

		BEFORE_EACH()
		{
			// Combat Bible §3's example constant; the game reads it from Game/Tuning/Combat.json.
			Tuning.MitigationConstant = 100.0;
		}

		FVeyraMitigatedDamage Resolve(const FVeyraDamageComponents& Components, const FVeyraAttackerOffence& Offence, const FVeyraDefenderDefence& Defence) const
		{
			FVeyraRawDamageEvent Raw;
			Raw.Components = Components;
			return VeyraDamage::ApplyTargetSide(VeyraDamage::ApplySourceSide(Raw, Offence), Defence, Tuning);
		}

		TEST_METHOD(AmplificationAndReductionStackMultiplicatively)
		{
			FVeyraAttackerOffence Offence;
			Offence.OutgoingDamageMultiplier = 1.2 * 1.1;
			FVeyraDefenderDefence Defence;
			Defence.IncomingDamageMultiplier = 0.8 * 0.9;

			const FVeyraMitigatedDamage Result = Resolve({ { EVeyraDamageType::Physical, 100.0 }, { EVeyraDamageType::Magic, 100.0 } }, Offence, Defence);
			ASSERT_THAT(IsNear(100.0 * 1.32 * 0.72, Result.Components[0].Amount, Tolerance));
			ASSERT_THAT(IsNear(100.0 * 1.32 * 0.72, Result.Components[1].Amount, Tolerance));
		}

		TEST_METHOD(TrueDamageIgnoresAmplificationMitigationAndReduction)
		{
			FVeyraAttackerOffence Offence;
			Offence.OutgoingDamageMultiplier = 1.5;
			FVeyraDefenderDefence Defence;
			Defence.Armor = 100.0;
			Defence.MagicResist = 100.0;
			Defence.IncomingDamageMultiplier = 0.5;

			const FVeyraMitigatedDamage Result = Resolve({ { EVeyraDamageType::TrueDamage, 100.0 } }, Offence, Defence);
			ASSERT_THAT(IsTrue(Result.Components[0].Amount == 100.0, TEXT("True Damage changed")));
		}

		TEST_METHOD(ComponentsResolveIndependently)
		{
			FVeyraDefenderDefence Defence;
			Defence.Armor = 100.0;
			Defence.MagicResist = 0.0;

			const FVeyraMitigatedDamage Result = Resolve(
				{ { EVeyraDamageType::Physical, 100.0 }, { EVeyraDamageType::Magic, 100.0 }, { EVeyraDamageType::TrueDamage, 50.0 } },
				FVeyraAttackerOffence(), Defence);
			ASSERT_THAT(AreEqual(3, Result.Components.Num()));
			ASSERT_THAT(IsNear(50.0, Result.Components[0].Amount, Tolerance));
			ASSERT_THAT(IsNear(100.0, Result.Components[1].Amount, Tolerance));
			ASSERT_THAT(IsNear(50.0, Result.Components[2].Amount, Tolerance));
		}

		TEST_METHOD(ZeroDamageIsValid)
		{
			const FVeyraMitigatedDamage Result = Resolve({ { EVeyraDamageType::Physical, 0.0 } }, FVeyraAttackerOffence(), FVeyraDefenderDefence());
			ASSERT_THAT(IsTrue(Result.Components[0].Amount == 0.0));
		}

		TEST_METHOD(DamageIsNeverNegative)
		{
			FVeyraAttackerOffence NoAmplification;
			NoAmplification.OutgoingDamageMultiplier = -1.0;
			FVeyraDefenderDefence NegativeReduction;
			NegativeReduction.IncomingDamageMultiplier = -0.5;

			ASSERT_THAT(IsTrue(Resolve({ { EVeyraDamageType::Physical, 100.0 } }, NoAmplification, FVeyraDefenderDefence()).Components[0].Amount == 0.0));
			ASSERT_THAT(IsTrue(Resolve({ { EVeyraDamageType::Magic, 100.0 } }, FVeyraAttackerOffence(), NegativeReduction).Components[0].Amount == 0.0));
			ASSERT_THAT(IsTrue(Resolve({ { EVeyraDamageType::TrueDamage, -5.0 } }, FVeyraAttackerOffence(), FVeyraDefenderDefence()).Components[0].Amount == 0.0));
		}

		TEST_METHOD(ResultsAreNotRounded)
		{
			FVeyraDefenderDefence Defence;
			Defence.Armor = 50.0;
			const double Actual = Resolve({ { EVeyraDamageType::Physical, 10.0 } }, FVeyraAttackerOffence(), Defence).Components[0].Amount;
			ASSERT_THAT(IsTrue(Actual == 10.0 * (100.0 / 150.0), FString::Printf(TEXT("Got %.17g"), Actual)));
			ASSERT_THAT(IsTrue(FMath::Frac(Actual) > 0.0, TEXT("The result was rounded to a whole number")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
