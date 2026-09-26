// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Absorption/VeyraAbsorptionLedger.h"
#include "CQTest.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCombatTests
{
	// Veyra.Combat.Absorption.*: Combat Bible §25 steps 7–9 — Invulnerability, shield priority (§7)
	// and Temporary Health (§7).
	TEST_CLASS(Absorption, "Veyra.Combat")
	{
		static constexpr double Tolerance = 1e-12;
		static constexpr double FullHealth = 1000.0;

		static FVeyraShieldEntry Shield(int32 Sequence, EVeyraShieldCategory Category, double Amount)
		{
			FVeyraShieldEntry Entry;
			Entry.Sequence = Sequence;
			Entry.Category = Category;
			Entry.Remaining = Amount;
			return Entry;
		}

		static FVeyraTemporaryHealthGrant Grant(int32 Sequence, double Amount)
		{
			FVeyraTemporaryHealthGrant Entry;
			Entry.Sequence = Sequence;
			Entry.Remaining = Amount;
			return Entry;
		}

		static const FVeyraShieldEntry* FindShield(const FVeyraAbsorptionLedger& Ledger, int32 Sequence)
		{
			return Ledger.Shields.FindByPredicate([Sequence](const FVeyraShieldEntry& Entry) { return Entry.Sequence == Sequence; });
		}

		TEST_METHOD(PhysicalDamageUsesPhysicalShieldsBeforeUniversal)
		{
			FVeyraAbsorptionLedger Ledger;
			Ledger.Shields = { Shield(1, EVeyraShieldCategory::Universal, 50.0), Shield(2, EVeyraShieldCategory::Physical, 30.0) };

			const FVeyraAbsorptionResult Result = VeyraAbsorption::Absorb(EVeyraDamageType::Physical, 40.0, false, Ledger, FullHealth);
			ASSERT_THAT(IsNear(40.0, Result.ShieldAbsorbed, Tolerance));
			ASSERT_THAT(IsTrue(Result.DepletedShields == TArray<int32>{ 2 }, TEXT("The newer Physical shield should be emptied first")));
			ASSERT_THAT(IsNear(40.0, FindShield(Ledger, 1)->Remaining, Tolerance));
			ASSERT_THAT(IsTrue(Result.HealthLost == 0.0));
		}

		TEST_METHOD(MagicDamageUsesMagicShieldsBeforeUniversal)
		{
			FVeyraAbsorptionLedger Ledger;
			Ledger.Shields = { Shield(1, EVeyraShieldCategory::Universal, 50.0), Shield(2, EVeyraShieldCategory::Magic, 30.0),
				Shield(3, EVeyraShieldCategory::Physical, 100.0) };

			const FVeyraAbsorptionResult Result = VeyraAbsorption::Absorb(EVeyraDamageType::Magic, 40.0, false, Ledger, FullHealth);
			ASSERT_THAT(IsTrue(Result.DepletedShields == TArray<int32>{ 2 }));
			ASSERT_THAT(IsNear(40.0, FindShield(Ledger, 1)->Remaining, Tolerance));
			ASSERT_THAT(IsNear(100.0, FindShield(Ledger, 3)->Remaining, Tolerance, TEXT("A Physical shield never absorbs Magic Damage")));
		}

		TEST_METHOD(TrueDamageUsesUniversalShieldsOnly)
		{
			FVeyraAbsorptionLedger Ledger;
			Ledger.Shields = { Shield(1, EVeyraShieldCategory::Physical, 100.0), Shield(2, EVeyraShieldCategory::Magic, 100.0),
				Shield(3, EVeyraShieldCategory::Universal, 30.0) };

			const FVeyraAbsorptionResult Result = VeyraAbsorption::Absorb(EVeyraDamageType::True, 50.0, false, Ledger, FullHealth);
			ASSERT_THAT(IsNear(30.0, Result.ShieldAbsorbed, Tolerance));
			ASSERT_THAT(IsNear(20.0, Result.HealthLost, Tolerance));
			ASSERT_THAT(IsNear(100.0, FindShield(Ledger, 1)->Remaining, Tolerance));
			ASSERT_THAT(IsNear(100.0, FindShield(Ledger, 2)->Remaining, Tolerance));
			ASSERT_THAT(IsTrue(FindShield(Ledger, 3) == nullptr, TEXT("The emptied Universal shield should be removed")));
		}

		TEST_METHOD(SpendsTheOldestShieldFirstWithinACategory)
		{
			FVeyraAbsorptionLedger Ledger;
			Ledger.Shields = { Shield(5, EVeyraShieldCategory::Universal, 20.0), Shield(2, EVeyraShieldCategory::Universal, 20.0) };

			const FVeyraAbsorptionResult Result = VeyraAbsorption::Absorb(EVeyraDamageType::Magic, 25.0, false, Ledger, FullHealth);
			ASSERT_THAT(IsTrue(Result.DepletedShields == TArray<int32>{ 2 }));
			ASSERT_THAT(IsNear(15.0, FindShield(Ledger, 5)->Remaining, Tolerance));
		}

		TEST_METHOD(SpendsTemporaryHealthAfterShieldsForEveryType)
		{
			for (const EVeyraDamageType Type : { EVeyraDamageType::Physical, EVeyraDamageType::Magic, EVeyraDamageType::True })
			{
				FVeyraAbsorptionLedger Ledger;
				Ledger.Shields = { Shield(1, EVeyraShieldCategory::Universal, 10.0) };
				Ledger.TemporaryHealth = { Grant(3, 20.0), Grant(2, 20.0) };

				const FVeyraAbsorptionResult Result = VeyraAbsorption::Absorb(Type, 35.0, false, Ledger, FullHealth);
				ASSERT_THAT(IsNear(10.0, Result.ShieldAbsorbed, Tolerance));
				ASSERT_THAT(IsNear(25.0, Result.TemporaryHealthSpent, Tolerance));
				ASSERT_THAT(IsTrue(Result.DepletedTemporaryHealth == TArray<int32>{ 2 }, TEXT("The oldest grant is spent first")));
				ASSERT_THAT(IsNear(15.0, VeyraAbsorption::TotalTemporaryHealth(Ledger), Tolerance));
				ASSERT_THAT(IsTrue(Result.HealthLost == 0.0, TEXT("Ordinary Health is spent last")));
			}
		}

		TEST_METHOD(HealthStopsAtZeroAndReportsOverkill)
		{
			FVeyraAbsorptionLedger Ledger;
			const FVeyraAbsorptionResult Result = VeyraAbsorption::Absorb(EVeyraDamageType::Physical, 50.0, false, Ledger, 30.0);
			ASSERT_THAT(IsNear(30.0, Result.HealthLost, Tolerance));
			ASSERT_THAT(IsNear(20.0, Result.Overkill, Tolerance));
		}

		TEST_METHOD(InvulnerabilityConsumesNothing)
		{
			FVeyraAbsorptionLedger Ledger;
			Ledger.Shields = { Shield(1, EVeyraShieldCategory::Universal, 50.0) };
			Ledger.TemporaryHealth = { Grant(2, 50.0) };

			const FVeyraAbsorptionResult Result = VeyraAbsorption::Absorb(EVeyraDamageType::True, 500.0, true, Ledger, FullHealth);
			ASSERT_THAT(IsTrue(Result.bBlockedByInvulnerability));
			ASSERT_THAT(IsTrue(Result.ShieldAbsorbed == 0.0 && Result.TemporaryHealthSpent == 0.0 && Result.HealthLost == 0.0));
			ASSERT_THAT(IsNear(50.0, FindShield(Ledger, 1)->Remaining, Tolerance));
			ASSERT_THAT(IsNear(50.0, VeyraAbsorption::TotalTemporaryHealth(Ledger), Tolerance));
		}

		TEST_METHOD(ZeroDamageConsumesNothing)
		{
			FVeyraAbsorptionLedger Ledger;
			Ledger.Shields = { Shield(1, EVeyraShieldCategory::Universal, 50.0) };
			const FVeyraAbsorptionResult Result = VeyraAbsorption::Absorb(EVeyraDamageType::Physical, 0.0, false, Ledger, FullHealth);
			ASSERT_THAT(IsTrue(Result.ShieldAbsorbed == 0.0 && Result.HealthLost == 0.0 && Result.DepletedShields.IsEmpty()));
			ASSERT_THAT(AreEqual(1, Ledger.Shields.Num()));
		}

		TEST_METHOD(TotalHealthLostDoesNotDependOnComponentOrder)
		{
			FVeyraAbsorptionLedger Start;
			Start.Shields = { Shield(1, EVeyraShieldCategory::Physical, 20.0), Shield(2, EVeyraShieldCategory::Magic, 20.0),
				Shield(3, EVeyraShieldCategory::Universal, 50.0) };
			Start.TemporaryHealth = { Grant(4, 30.0) };

			const TArray<EVeyraDamageType> Forward = { EVeyraDamageType::Physical, EVeyraDamageType::Magic, EVeyraDamageType::True };
			const TArray<EVeyraDamageType> Backward = { EVeyraDamageType::True, EVeyraDamageType::Magic, EVeyraDamageType::Physical };
			double HealthLost[2] = { 0.0, 0.0 };
			const TArray<EVeyraDamageType>* Orders[2] = { &Forward, &Backward };
			for (int32 Run = 0; Run < 2; ++Run)
			{
				FVeyraAbsorptionLedger Ledger = Start;
				double Health = FullHealth;
				for (const EVeyraDamageType Type : *Orders[Run])
				{
					const FVeyraAbsorptionResult Result = VeyraAbsorption::Absorb(Type, 60.0, false, Ledger, Health);
					Health -= Result.HealthLost;
					HealthLost[Run] += Result.HealthLost;
				}
			}
			ASSERT_THAT(IsNear(HealthLost[0], HealthLost[1], Tolerance));
			ASSERT_THAT(IsNear(60.0, HealthLost[0], Tolerance));
		}

		TEST_METHOD(TemporaryHealthCountsAsHealth)
		{
			FVeyraAbsorptionLedger Ledger;
			Ledger.TemporaryHealth = { Grant(1, 100.0), Grant(2, 50.0) };
			ASSERT_THAT(IsNear(650.0, VeyraAbsorption::GetEffectiveHealth(500.0, Ledger), Tolerance));
			ASSERT_THAT(IsNear(1150.0, VeyraAbsorption::GetEffectiveMaxHealth(1000.0, Ledger), Tolerance));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
