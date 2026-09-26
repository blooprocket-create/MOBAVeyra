// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attributes/VeyraResourceSet.h"
#include "CQTest.h"
#include "Tests/Combat/VeyraCombatTestHelpers.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCombatTests
{
	// Veyra.Combat.ResourceCosts.*: ability costs spend the resource, which never goes negative (Combat
	// Bible §27).
	TEST_CLASS(ResourceCosts, "Veyra.Combat")
	{
		static constexpr double StartingMaxResource = 100.0;

		FActorTestSpawner Spawner;
		UAbilitySystemComponent* Unit = nullptr;

		BEFORE_EACH()
		{
			Unit = &SpawnCombatant(Spawner);
			ASSERT_THAT(IsTrue(VeyraCombat::InitializeResource(*Unit, StartingMaxResource)));
		}

		double Resource() const
		{
			return Unit->GetNumericAttribute(UVeyraResourceSet::GetResourceAttribute());
		}

		TEST_METHOD(InitializingFillsTheResource)
		{
			ASSERT_THAT(IsTrue(Resource() == StartingMaxResource));
			ASSERT_THAT(IsTrue(Unit->GetNumericAttribute(UVeyraResourceSet::GetMaxResourceAttribute()) == StartingMaxResource));
		}

		TEST_METHOD(SpendingTakesTheCost)
		{
			constexpr double Cost = 30.0;
			ASSERT_THAT(IsTrue(VeyraCombat::SpendResource(*Unit, Cost)));
			ASSERT_THAT(IsTrue(Resource() == StartingMaxResource - Cost));
			ASSERT_THAT(IsTrue(VeyraCombat::CanAffordResource(*Unit, StartingMaxResource - Cost)));
			ASSERT_THAT(IsFalse(VeyraCombat::CanAffordResource(*Unit, StartingMaxResource)));
		}

		TEST_METHOD(AnUnaffordableCostChangesNothing)
		{
			ASSERT_THAT(IsFalse(VeyraCombat::SpendResource(*Unit, StartingMaxResource + 1.0)));
			ASSERT_THAT(IsTrue(Resource() == StartingMaxResource));
		}

		TEST_METHOD(AFreeCastNeedsNoResource)
		{
			ASSERT_THAT(IsTrue(VeyraCombat::InitializeResource(*Unit, 0.0)));
			ASSERT_THAT(IsTrue(VeyraCombat::CanAffordResource(*Unit, 0.0)));
			ASSERT_THAT(IsTrue(VeyraCombat::SpendResource(*Unit, 0.0)));
		}

		TEST_METHOD(RefusesAnInvalidCost)
		{
			TestRunner->AddExpectedMessagePlain(TEXT("Refused a resource cost"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 2);
			ASSERT_THAT(IsFalse(VeyraCombat::SpendResource(*Unit, -1.0)));
			ASSERT_THAT(IsFalse(VeyraCombat::SpendResource(*Unit, NAN)));
			ASSERT_THAT(IsTrue(Resource() == StartingMaxResource));
		}

		TEST_METHOD(ALowerMaximumCapsTheResource)
		{
			constexpr double LowerMaximum = 40.0;
			ASSERT_THAT(IsTrue(VeyraCombat::InitializeResource(*Unit, LowerMaximum)));
			Unit->SetNumericAttributeBase(UVeyraResourceSet::GetMaxResourceAttribute(), static_cast<float>(LowerMaximum / 2.0));
			ASSERT_THAT(IsTrue(Resource() == LowerMaximum / 2.0));
		}

		TEST_METHOD(EffectsCannotWriteTheResource)
		{
			TestRunner->AddExpectedMessagePlain(TEXT("cannot be changed by a modifier"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1);
			const FActiveGameplayEffectHandle Handle = ApplyToSelf(*Unit, NewTestEffect(EGameplayEffectDurationType::Infinite,
				{ { UVeyraResourceSet::GetResourceAttribute(), EGameplayModOp::AddBase, 10.0f } }));
			ASSERT_THAT(IsFalse(Handle.WasSuccessfullyApplied()));
			ASSERT_THAT(IsTrue(Resource() == StartingMaxResource));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
