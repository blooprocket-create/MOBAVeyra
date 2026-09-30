// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attributes/VeyraDefenceSet.h"
#include "Attributes/VeyraMobilitySet.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Attributes/VeyraResourceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "CQTest.h"
#include "Effects/VeyraCombatEffects.h"
#include "Life/VeyraLifeComponent.h"
#include "Regeneration/VeyraRegenerationComponent.h"
#include "Stats/VeyraHaste.h"
#include "Tests/Combat/VeyraCombatTestHelpers.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCombatTests
{
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

	// Veyra.Combat.EquipmentStats.*: what equipment adds to a unit, replaced whole with each change
	// (ADR-012 §6; Combat Bible §41).
	TEST_CLASS(EquipmentStats, "Veyra.Combat")
	{
		// Fixture values.
		static constexpr double HealthLost = 100.0;
		static constexpr double Tolerance = 1e-3;

		FActorTestSpawner Spawner;
		UAbilitySystemComponent* Unit = nullptr;
		FVeyraStatBlock Base;

		BEFORE_EACH()
		{
			Unit = &SpawnCombatant(Spawner);
			Base = ExampleStats();
			ASSERT_THAT(IsTrue(VeyraCombat::InitializeStats(*Unit, Base)));
			Unit->SetNumericAttributeBase(UVeyraVitalsSet::GetHealthAttribute(), static_cast<float>(Base.MaxHealth - HealthLost));
		}

		double Value(const FGameplayAttribute& Attribute) const
		{
			return Unit->GetNumericAttribute(Attribute);
		}

		bool Near(double A, double B) const
		{
			return FMath::IsNearlyEqual(A, B, Tolerance);
		}

		TEST_METHOD(EquipmentAddsFlatStatsAndAPercentageOfMagicPower)
		{
			FVeyraEquipmentStats Equipment;
			Equipment.MaxHealth = 200.0;
			Equipment.PhysicalPower = 15.0;
			Equipment.MagicPower = 30.0;
			Equipment.MagicPowerFraction = 0.5;
			Equipment.AttackSpeed = 0.1;
			Equipment.AbilityHaste = 20.0;
			Equipment.MoveSpeed = 25.0;
			Equipment.MagicPenetrationFlat = 10.0;
			Equipment.CritChance = 0.35;
			Equipment.CritDamageBonus = 0.4;
			ASSERT_THAT(IsTrue(VeyraCombat::SetEquipmentStats(*Unit, Equipment)));
			ASSERT_THAT(IsTrue(Near(Value(UVeyraOffenceSet::GetCritChanceAttribute()), Equipment.CritChance), TEXT("crit adds (ADR-022 §2)")));
			ASSERT_THAT(IsTrue(Near(Value(UVeyraOffenceSet::GetCritDamageBonusAttribute()), Equipment.CritDamageBonus)));
			ASSERT_THAT(IsTrue(Near(Value(UVeyraVitalsSet::GetMaxHealthAttribute()), Base.MaxHealth + Equipment.MaxHealth)));
			ASSERT_THAT(IsTrue(Near(Value(UVeyraOffenceSet::GetPhysicalPowerAttribute()), Base.PhysicalPower + Equipment.PhysicalPower)));
			ASSERT_THAT(IsTrue(Near(Value(UVeyraOffenceSet::GetMagicPowerAttribute()), (Base.MagicPower + Equipment.MagicPower) * (1.0 + Equipment.MagicPowerFraction)),
				TEXT("flat bonuses first, then the percentage (§41)")));
			ASSERT_THAT(IsTrue(Near(Value(UVeyraOffenceSet::GetAttackSpeedAttribute()), Base.AttackSpeed + Equipment.AttackSpeed)));
			ASSERT_THAT(IsTrue(Near(Value(UVeyraOffenceSet::GetAbilityHasteAttribute()), Equipment.AbilityHaste)));
			ASSERT_THAT(IsTrue(Near(Value(UVeyraMobilitySet::GetMoveSpeedAttribute()), Base.MoveSpeed + Equipment.MoveSpeed)));
			ASSERT_THAT(IsTrue(Near(Value(UVeyraOffenceSet::GetMagicPenetrationFlatAttribute()), Equipment.MagicPenetrationFlat)));
		}

		TEST_METHOD(HealthKeepsItsPercentage)
		{
			const double Fraction = (Base.MaxHealth - HealthLost) / Base.MaxHealth;
			FVeyraEquipmentStats Equipment;
			Equipment.MaxHealth = 300.0;
			ASSERT_THAT(IsTrue(VeyraCombat::SetEquipmentStats(*Unit, Equipment)));
			ASSERT_THAT(IsTrue(Near(Value(UVeyraVitalsSet::GetHealthAttribute()), (Base.MaxHealth + Equipment.MaxHealth) * Fraction)));
			ASSERT_THAT(IsTrue(VeyraCombat::SetEquipmentStats(*Unit, FVeyraEquipmentStats())));
			ASSERT_THAT(IsTrue(Near(Value(UVeyraVitalsSet::GetHealthAttribute()), Base.MaxHealth * Fraction), TEXT("and keeps it as the equipment goes")));
		}

		TEST_METHOD(EachChangeReplacesTheLast)
		{
			FVeyraEquipmentStats First;
			First.PhysicalPower = 30.0;
			First.AbilityHaste = 10.0;
			ASSERT_THAT(IsTrue(VeyraCombat::SetEquipmentStats(*Unit, First)));
			FVeyraEquipmentStats Second;
			Second.PhysicalPower = 5.0;
			ASSERT_THAT(IsTrue(VeyraCombat::SetEquipmentStats(*Unit, Second)));
			ASSERT_THAT(IsTrue(Near(Value(UVeyraOffenceSet::GetPhysicalPowerAttribute()), Base.PhysicalPower + Second.PhysicalPower)));
			ASSERT_THAT(IsTrue(Value(UVeyraOffenceSet::GetAbilityHasteAttribute()) == 0.0));

			ASSERT_THAT(IsTrue(VeyraCombat::SetEquipmentStats(*Unit, FVeyraEquipmentStats())));
			FGameplayEffectQuery Equipment;
			Equipment.EffectDefinition = UVeyraEquipmentEffect::StaticClass();
			ASSERT_THAT(IsTrue(Unit->GetActiveEffects(Equipment).IsEmpty(), TEXT("no equipment, no effect")));
		}

		TEST_METHOD(RefusesNegativeStats)
		{
			TestRunner->AddExpectedMessagePlain(TEXT("Refused equipment stats"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1);
			FVeyraEquipmentStats Negative;
			Negative.PhysicalPower = -1.0;
			ASSERT_THAT(IsFalse(VeyraCombat::SetEquipmentStats(*Unit, Negative)));
			ASSERT_THAT(IsTrue(Value(UVeyraOffenceSet::GetPhysicalPowerAttribute()) == Base.PhysicalPower));
		}
	};

	// Veyra.Combat.Haste.*: Haste's cooldown arithmetic (Combat Bible §21, §39).
	TEST_CLASS(Haste, "Veyra.Combat")
	{
		TEST_METHOD(HasteShortensCooldownsWithAFloorOfZero)
		{
			ASSERT_THAT(IsTrue(VeyraHaste::CooldownMultiplier(0.0) == 1.0));
			ASSERT_THAT(IsTrue(VeyraHaste::CooldownMultiplier(100.0) == 0.5, TEXT("the bible's 100 Haste halves a cooldown")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraHaste::CooldownMultiplier(25.0), 0.8)));
			ASSERT_THAT(IsTrue(VeyraHaste::CooldownMultiplier(-50.0) == 1.0, TEXT("never a longer cooldown")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
