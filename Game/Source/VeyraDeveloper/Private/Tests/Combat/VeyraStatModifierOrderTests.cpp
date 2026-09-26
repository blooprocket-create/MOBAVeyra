// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attributes/VeyraDefenceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "CQTest.h"
#include "Tests/Combat/VeyraCombatTestHelpers.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCombatTests
{
	// Veyra.Combat.StatModifierOrder.*: Combat Bible §41 through the real Gameplay Ability System
	// aggregation, on a PlayerState's Ability System Component (ADR-006 §4).
	TEST_CLASS(StatModifierOrder, "Veyra.Combat")
	{
		static constexpr double Tolerance = 1e-3;

		FActorTestSpawner Spawner;

		static double Value(const UAbilitySystemComponent& Unit, const FGameplayAttribute& Attribute)
		{
			return Unit.GetNumericAttribute(Attribute);
		}

		static double Armor(const UAbilitySystemComponent& Unit)
		{
			return Value(Unit, UVeyraDefenceSet::GetArmorAttribute());
		}

		TEST_METHOD(AppliesFlatBonusesThenMultipliesPercentages)
		{
			UAbilitySystemComponent& Unit = SpawnCombatant(Spawner);
			Unit.SetNumericAttributeBase(UVeyraDefenceSet::GetArmorAttribute(), 1000.0f);
			const FGameplayAttribute ArmorAttribute = UVeyraDefenceSet::GetArmorAttribute();

			const FActiveGameplayEffectHandle Handle = ApplyToSelf(Unit, NewTestEffect(EGameplayEffectDurationType::Infinite, {
				{ ArmorAttribute, EGameplayModOp::AddBase, 100.0f },
				{ ArmorAttribute, EGameplayModOp::MultiplyCompound, 1.2f },
				{ ArmorAttribute, EGameplayModOp::MultiplyCompound, 1.1f },
				{ ArmorAttribute, EGameplayModOp::MultiplyCompound, 0.8f },
			}));
			ASSERT_THAT(IsTrue(Handle.IsValid()));
			ASSERT_THAT(IsNear((1000.0 + 100.0) * 1.2 * 1.1 * 0.8, Armor(Unit), Tolerance));

			ASSERT_THAT(IsTrue(Unit.RemoveActiveGameplayEffect(Handle)));
			ASSERT_THAT(IsNear(1000.0, Armor(Unit), Tolerance));
		}

		TEST_METHOD(MultipliesPercentagesFromSeparateSources)
		{
			// Two +20% sources give ×1.44. The Gameplay Ability System's default adds them to ×1.4.
			UAbilitySystemComponent& Unit = SpawnCombatant(Spawner);
			Unit.SetNumericAttributeBase(UVeyraDefenceSet::GetArmorAttribute(), 100.0f);
			for (int32 Source = 0; Source < 2; ++Source)
			{
				ApplyToSelf(Unit, NewTestEffect(EGameplayEffectDurationType::Infinite, { { UVeyraDefenceSet::GetArmorAttribute(), EGameplayModOp::MultiplyCompound, 1.2f } }));
			}
			ASSERT_THAT(IsNear(144.0, Armor(Unit), Tolerance));
		}

		TEST_METHOD(ChangingMaxHealthKeepsTheHealthPercentage)
		{
			UAbilitySystemComponent& Unit = SpawnCombatant(Spawner);
			ASSERT_THAT(IsTrue(VeyraCombat::InitializeVitals(Unit, 1000.0)));
			Unit.SetNumericAttributeBase(UVeyraVitalsSet::GetHealthAttribute(), 500.0f);

			ApplyToSelf(Unit, NewTestEffect(EGameplayEffectDurationType::Infinite, { { UVeyraVitalsSet::GetMaxHealthAttribute(), EGameplayModOp::MultiplyCompound, 1.2f } }));
			ASSERT_THAT(IsNear(1200.0, Value(Unit, UVeyraVitalsSet::GetMaxHealthAttribute()), Tolerance));
			ASSERT_THAT(IsNear(600.0, Value(Unit, UVeyraVitalsSet::GetHealthAttribute()), Tolerance));
		}

		TEST_METHOD(RejectsAdditivePercentageModifiers)
		{
			UAbilitySystemComponent& Unit = SpawnCombatant(Spawner);
			Unit.SetNumericAttributeBase(UVeyraDefenceSet::GetArmorAttribute(), 100.0f);
			TestRunner->AddExpectedMessagePlain(TEXT("cannot use"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1);

			const FActiveGameplayEffectHandle Handle = ApplyToSelf(Unit, NewTestEffect(EGameplayEffectDurationType::Infinite,
				{ { UVeyraDefenceSet::GetArmorAttribute(), EGameplayModOp::MultiplyAdditive, 1.2f } }));
			ASSERT_THAT(IsFalse(Handle.WasSuccessfullyApplied()));
			ASSERT_THAT(IsNear(100.0, Armor(Unit), Tolerance));
		}

		TEST_METHOD(RejectsModifiersOnHealth)
		{
			UAbilitySystemComponent& Unit = SpawnCombatant(Spawner);
			ASSERT_THAT(IsTrue(VeyraCombat::InitializeVitals(Unit, 1000.0)));
			TestRunner->AddExpectedMessagePlain(TEXT("cannot be changed by a modifier"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1);

			const FActiveGameplayEffectHandle Handle = ApplyToSelf(Unit, NewTestEffect(EGameplayEffectDurationType::Infinite,
				{ { UVeyraVitalsSet::GetHealthAttribute(), EGameplayModOp::AddBase, -100.0f } }));
			ASSERT_THAT(IsFalse(Handle.WasSuccessfullyApplied()));
			ASSERT_THAT(IsNear(1000.0, Value(Unit, UVeyraVitalsSet::GetHealthAttribute()), Tolerance));
		}

		TEST_METHOD(KeepsMaxHealthAboveZero)
		{
			UAbilitySystemComponent& Unit = SpawnCombatant(Spawner);
			ASSERT_THAT(IsTrue(VeyraCombat::InitializeVitals(Unit, 1000.0)));
			// Occurrences 0: at least once; the aggregator may re-evaluate more than once.
			TestRunner->AddExpectedMessagePlain(TEXT("must stay above 0"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 0);

			ApplyToSelf(Unit, NewTestEffect(EGameplayEffectDurationType::Infinite, { { UVeyraVitalsSet::GetMaxHealthAttribute(), EGameplayModOp::MultiplyCompound, 0.0f } }));
			ASSERT_THAT(IsNear(1000.0, Value(Unit, UVeyraVitalsSet::GetMaxHealthAttribute()), Tolerance));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
