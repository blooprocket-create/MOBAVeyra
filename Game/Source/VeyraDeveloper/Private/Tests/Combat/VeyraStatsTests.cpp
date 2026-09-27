// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attributes/VeyraDefenceSet.h"
#include "Attributes/VeyraMobilitySet.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Attributes/VeyraResourceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "CQTest.h"
#include "Life/VeyraLifeComponent.h"
#include "Regeneration/VeyraRegenerationComponent.h"
#include "Tests/Combat/VeyraCombatTestHelpers.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCombatTests
{
	/** A complete, valid stat block. Test fixture values, not tuning. */
	inline FVeyraStatBlock ExampleStats()
	{
		FVeyraStatBlock Stats;
		Stats.MaxHealth = 600.0;
		Stats.MaxResource = 300.0;
		Stats.ResourceRegen = 4.0;
		Stats.Armor = 30.0;
		Stats.MagicResist = 25.0;
		Stats.PhysicalPower = 55.0;
		Stats.MagicPower = 10.0;
		Stats.AttackSpeed = 0.65;
		Stats.MoveSpeed = 335.0;
		return Stats;
	}

	// Veyra.Combat.Stats.*: base stats from data, and level-up growth that keeps what is missing
	// (ADR-008 §2, §6; Economy & Progression Bible §9).
	TEST_CLASS(Stats, "Veyra.Combat")
	{
		FActorTestSpawner Spawner;
		UAbilitySystemComponent* Unit = nullptr;

		BEFORE_EACH()
		{
			Unit = &SpawnCombatant(Spawner);
		}

		double Value(const FGameplayAttribute& Attribute) const
		{
			return Unit->GetNumericAttribute(Attribute);
		}

		TEST_METHOD(InitializingSetsEveryStatAndFillsThePools)
		{
			const FVeyraStatBlock Stats = ExampleStats();
			ASSERT_THAT(IsTrue(VeyraCombat::InitializeStats(*Unit, Stats)));
			ASSERT_THAT(IsTrue(Value(UVeyraVitalsSet::GetMaxHealthAttribute()) == Stats.MaxHealth));
			ASSERT_THAT(IsTrue(Value(UVeyraVitalsSet::GetHealthAttribute()) == Stats.MaxHealth));
			ASSERT_THAT(IsTrue(Value(UVeyraResourceSet::GetMaxResourceAttribute()) == Stats.MaxResource));
			ASSERT_THAT(IsTrue(Value(UVeyraResourceSet::GetResourceAttribute()) == Stats.MaxResource));
			ASSERT_THAT(IsTrue(Value(UVeyraResourceSet::GetResourceRegenAttribute()) == static_cast<float>(Stats.ResourceRegen)));
			ASSERT_THAT(IsTrue(Value(UVeyraDefenceSet::GetArmorAttribute()) == Stats.Armor));
			ASSERT_THAT(IsTrue(Value(UVeyraDefenceSet::GetMagicResistAttribute()) == Stats.MagicResist));
			ASSERT_THAT(IsTrue(Value(UVeyraOffenceSet::GetPhysicalPowerAttribute()) == Stats.PhysicalPower));
			ASSERT_THAT(IsTrue(Value(UVeyraOffenceSet::GetMagicPowerAttribute()) == Stats.MagicPower));
			ASSERT_THAT(IsTrue(Value(UVeyraOffenceSet::GetAttackSpeedAttribute()) == static_cast<float>(Stats.AttackSpeed)));
			ASSERT_THAT(IsTrue(Value(UVeyraMobilitySet::GetMoveSpeedAttribute()) == Stats.MoveSpeed));
		}

		TEST_METHOD(RefusesStatsOutsideTheirRanges)
		{
			TestRunner->AddExpectedMessagePlain(TEXT("Refused to initialize the stats"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 3);
			FVeyraStatBlock NoHealth = ExampleStats();
			NoHealth.MaxHealth = 0.0;
			FVeyraStatBlock NegativeArmor = ExampleStats();
			NegativeArmor.Armor = -1.0;
			FVeyraStatBlock NoAttackSpeed = ExampleStats();
			NoAttackSpeed.AttackSpeed = 0.0;
			ASSERT_THAT(IsFalse(VeyraCombat::InitializeStats(*Unit, NoHealth)));
			ASSERT_THAT(IsFalse(VeyraCombat::InitializeStats(*Unit, NegativeArmor)));
			ASSERT_THAT(IsFalse(VeyraCombat::InitializeStats(*Unit, NoAttackSpeed)));
			ASSERT_THAT(IsTrue(Value(UVeyraDefenceSet::GetArmorAttribute()) == 0.0, TEXT("a refused initialization changed a stat")));
		}

		TEST_METHOD(GrowthKeepsWhatIsMissing)
		{
			constexpr double HealthLost = 100.0;
			constexpr double ResourceSpent = 50.0;
			const FVeyraStatBlock Stats = ExampleStats();
			ASSERT_THAT(IsTrue(VeyraCombat::InitializeStats(*Unit, Stats)));
			Unit->SetNumericAttributeBase(UVeyraVitalsSet::GetHealthAttribute(), static_cast<float>(Stats.MaxHealth - HealthLost));
			ASSERT_THAT(IsTrue(VeyraCombat::SpendResource(*Unit, ResourceSpent)));

			FVeyraStatBlock Growth;
			Growth.MaxHealth = 80.0;
			Growth.MaxResource = 20.0;
			Growth.Armor = 3.0;
			ASSERT_THAT(IsTrue(VeyraCombat::GrowBaseStats(*Unit, Growth)));
			ASSERT_THAT(IsTrue(Value(UVeyraVitalsSet::GetMaxHealthAttribute()) == Stats.MaxHealth + Growth.MaxHealth));
			ASSERT_THAT(IsTrue(Value(UVeyraVitalsSet::GetHealthAttribute()) == Stats.MaxHealth + Growth.MaxHealth - HealthLost,
				TEXT("a level-up should add the flat growth to current Health, not keep the percentage")));
			ASSERT_THAT(IsTrue(Value(UVeyraResourceSet::GetResourceAttribute()) == Stats.MaxResource + Growth.MaxResource - ResourceSpent));
			ASSERT_THAT(IsTrue(Value(UVeyraDefenceSet::GetArmorAttribute()) == Stats.Armor + Growth.Armor));
			ASSERT_THAT(IsTrue(Value(UVeyraMobilitySet::GetMoveSpeedAttribute()) == Stats.MoveSpeed, TEXT("a stat with no growth changed")));
		}

		TEST_METHOD(ADeadUnitStaysAtZeroHealthWhenItGrows)
		{
			const FVeyraStatBlock Stats = ExampleStats();
			ASSERT_THAT(IsTrue(VeyraCombat::InitializeStats(*Unit, Stats)));
			UVeyraLifeComponent* Life = Unit->GetOwner()->FindComponentByClass<UVeyraLifeComponent>();
			ASSERT_THAT(IsNotNull(Life));
			Unit->SetNumericAttributeBase(UVeyraVitalsSet::GetHealthAttribute(), 0.0f);
			ASSERT_THAT(IsTrue(Life->SetState(EVeyraLifeState::Dead)));

			FVeyraStatBlock Growth;
			Growth.MaxHealth = 80.0;
			ASSERT_THAT(IsTrue(VeyraCombat::GrowBaseStats(*Unit, Growth)));
			ASSERT_THAT(IsTrue(Value(UVeyraVitalsSet::GetMaxHealthAttribute()) == Stats.MaxHealth + Growth.MaxHealth));
			ASSERT_THAT(IsTrue(Value(UVeyraVitalsSet::GetHealthAttribute()) == 0.0));
		}

		TEST_METHOD(RefusesNegativeGrowth)
		{
			TestRunner->AddExpectedMessagePlain(TEXT("Refused to grow the stats"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1);
			const FVeyraStatBlock Stats = ExampleStats();
			ASSERT_THAT(IsTrue(VeyraCombat::InitializeStats(*Unit, Stats)));
			FVeyraStatBlock Growth;
			Growth.MaxHealth = 80.0;
			Growth.Armor = -3.0;
			ASSERT_THAT(IsFalse(VeyraCombat::GrowBaseStats(*Unit, Growth)));
			ASSERT_THAT(IsTrue(Value(UVeyraVitalsSet::GetMaxHealthAttribute()) == Stats.MaxHealth, TEXT("a refused growth changed a stat")));
		}

		TEST_METHOD(RestoringResourceStopsAtTheMaximum)
		{
			const FVeyraStatBlock Stats = ExampleStats();
			ASSERT_THAT(IsTrue(VeyraCombat::InitializeStats(*Unit, Stats)));
			ASSERT_THAT(IsTrue(VeyraCombat::SpendResource(*Unit, 10.0)));
			ASSERT_THAT(IsTrue(VeyraCombat::RestoreResource(*Unit, 4.0)));
			ASSERT_THAT(IsTrue(Value(UVeyraResourceSet::GetResourceAttribute()) == Stats.MaxResource - 6.0));
			ASSERT_THAT(IsTrue(VeyraCombat::RestoreResource(*Unit, 100.0)));
			ASSERT_THAT(IsTrue(Value(UVeyraResourceSet::GetResourceAttribute()) == Stats.MaxResource));
		}
	};

	// Veyra.Combat.Regeneration.*: the resource refills over time at the regeneration stat (Combat
	// Bible §28), and not while dead.
	TEST_CLASS(Regeneration, "Veyra.Combat")
	{
		FActorTestSpawner Spawner;
		UAbilitySystemComponent* Unit = nullptr;
		UVeyraRegenerationComponent* Regenerator = nullptr;

		BEFORE_EACH()
		{
			Unit = &SpawnCombatant(Spawner);
			Regenerator = Unit->GetOwner()->FindComponentByClass<UVeyraRegenerationComponent>();
			ASSERT_THAT(IsNotNull(Regenerator));
			ASSERT_THAT(IsTrue(VeyraCombat::InitializeStats(*Unit, ExampleStats())));
		}

		double Resource() const
		{
			return Unit->GetNumericAttribute(UVeyraResourceSet::GetResourceAttribute());
		}

		TEST_METHOD(ATickRestoresTheRateTimesItsLength)
		{
			constexpr double Spent = 50.0;
			constexpr double Seconds = 2.0;
			const FVeyraStatBlock Stats = ExampleStats();
			ASSERT_THAT(IsTrue(VeyraCombat::SpendResource(*Unit, Spent)));
			Regenerator->ApplyTick(Seconds);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Resource(), Stats.MaxResource - Spent + Stats.ResourceRegen * Seconds)));
		}

		TEST_METHOD(RegenerationStopsAtTheMaximum)
		{
			const FVeyraStatBlock Stats = ExampleStats();
			ASSERT_THAT(IsTrue(VeyraCombat::SpendResource(*Unit, 1.0)));
			Regenerator->ApplyTick(10.0);
			ASSERT_THAT(IsTrue(Resource() == Stats.MaxResource));
		}

		TEST_METHOD(TheDeadDoNotRegenerate)
		{
			constexpr double Spent = 50.0;
			ASSERT_THAT(IsTrue(VeyraCombat::SpendResource(*Unit, Spent)));
			const double Before = Resource();
			ASSERT_THAT(IsTrue(Unit->GetOwner()->FindComponentByClass<UVeyraLifeComponent>()->SetState(EVeyraLifeState::Dead)));
			Regenerator->ApplyTick(2.0);
			ASSERT_THAT(IsTrue(Resource() == Before));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
