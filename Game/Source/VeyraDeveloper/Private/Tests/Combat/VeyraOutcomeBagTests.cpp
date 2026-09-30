// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attacks/VeyraCombatRollSubsystem.h"
#include "Attacks/VeyraCrit.h"
#include "Components/ActorTestSpawner.h"
#include "CQTest.h"
#include "Engine/World.h"
#include "Random/VeyraOutcomeBag.h"
#include "Tests/Combat/VeyraCombatTestHelpers.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCombatTests
{
	// Veyra.Combat.OutcomeBags.*: chance drawn from shuffled bags, not rolled afresh (author ruling
	// 2026-09-30; ADR-022 §10). Every seed, size and chance here is a fixture value; the seeds make
	// each run the same.
	TEST_CLASS(OutcomeBags, "Veyra.Combat")
	{
		static constexpr int32 Seed = 20260930;
		static constexpr int32 Draws = 10;

		/** How many of one bag's draws succeed at Chance. */
		static int32 SuccessesInABag(FVeyraOutcomeBag& Bag, double Chance, int32 Size)
		{
			int32 Successes = 0;
			for (int32 Draw = 0; Draw < Size; ++Draw)
			{
				Successes += Bag.Check(Chance, Size) ? 1 : 0;
			}
			return Successes;
		}

		TEST_METHOD(EachBagHoldsOneValueFromEachSliceShuffled)
		{
			FVeyraOutcomeBag Bag(Seed);
			TArray<double> Values;
			for (int32 Draw = 0; Draw < Draws; ++Draw)
			{
				Values.Add(Bag.Next(Draws));
			}
			ASSERT_THAT(AreEqual(0, Bag.Remaining(), TEXT("a full bag drawn is empty")));
			TArray<double> Sorted = Values;
			Sorted.Sort();
			for (int32 Slice = 0; Slice < Draws; ++Slice)
			{
				ASSERT_THAT(AreEqual(Slice, FMath::FloorToInt32(Sorted[Slice] * Draws), FString::SanitizeFloat(Sorted[Slice])));
			}
			ASSERT_THAT(IsTrue(Values != Sorted, TEXT("drawn in a shuffled order")));
		}

		TEST_METHOD(AWholeShareLandsExactlyInEveryBag)
		{
			// 70% of a bag of ten is seven: every bag, in whatever order.
			constexpr double Chance = 0.7;
			constexpr int32 Bags = 200;
			FVeyraOutcomeBag Bag(Seed);
			for (int32 Each = 0; Each < Bags; ++Each)
			{
				ASSERT_THAT(AreEqual(7, SuccessesInABag(Bag, Chance, Draws)));
			}
		}

		TEST_METHOD(AnyChanceLandsWithinOneOfItsShareAndExactlyOnAverage)
		{
			constexpr int32 Bags = 4000;
			for (const double Chance : { 0.27, 0.335, 0.72, 0.05 })
			{
				FVeyraOutcomeBag Bag(Seed);
				const int32 Low = FMath::FloorToInt32(Chance * Draws);
				int32 Total = 0;
				for (int32 Each = 0; Each < Bags; ++Each)
				{
					const int32 Successes = SuccessesInABag(Bag, Chance, Draws);
					ASSERT_THAT(IsTrue(Successes == Low || Successes == Low + 1, FString::Printf(TEXT("%g: %d in a bag"), Chance, Successes)));
					Total += Successes;
				}
				const double Share = static_cast<double>(Total) / (Bags * Draws);
				ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Share, Chance, 0.005), FString::Printf(TEXT("%g landed %g of the time"), Chance, Share)));
			}
		}

		TEST_METHOD(NoneNeverLandsAndAllAlwaysDoes)
		{
			FVeyraOutcomeBag Bag(Seed);
			for (int32 Draw = 0; Draw < Draws * 20; ++Draw)
			{
				ASSERT_THAT(IsFalse(Bag.Check(0.0, Draws)));
				ASSERT_THAT(IsTrue(Bag.Check(1.0, Draws)));
			}
		}

		TEST_METHOD(AChanceThatChangesMidBagIsHonouredWithoutBias)
		{
			// A buff that comes and goes on every other draw: the share is their mean.
			constexpr double Low = 0.1;
			constexpr double High = 0.6;
			constexpr int32 Checks = 40000;
			FVeyraOutcomeBag Bag(Seed);
			int32 Successes = 0;
			for (int32 Draw = 0; Draw < Checks; ++Draw)
			{
				Successes += Bag.Check(Draw % 2 == 0 ? Low : High, Draws) ? 1 : 0;
			}
			const double Share = static_cast<double>(Successes) / Checks;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Share, (Low + High) / 2.0, 0.01), FString::SanitizeFloat(Share)));
		}

		TEST_METHOD(StreaksAreCutShort)
		{
			// A quarter in bags of four: one success a bag, so never more than three misses on either
			// side of a bag's end, and never two successes running within one bag.
			constexpr double Chance = 0.25;
			constexpr int32 Size = 4;
			constexpr int32 Checks = 20000;
			FVeyraOutcomeBag Bag(Seed);
			int32 Misses = 0;
			int32 LongestMisses = 0;
			for (int32 Draw = 0; Draw < Checks; ++Draw)
			{
				Misses = Bag.Check(Chance, Size) ? 0 : Misses + 1;
				LongestMisses = FMath::Max(LongestMisses, Misses);
			}
			ASSERT_THAT(IsTrue(LongestMisses <= 2 * (Size - 1), FString::FromInt(LongestMisses)));
		}

		TEST_METHOD(EachChannelKeepsItsOwnSequence)
		{
			FActorTestSpawner Spawner;
			const UAbilitySystemComponent& Unit = SpawnCombatant(Spawner);
			const UAbilitySystemComponent& Other = SpawnCombatant(Spawner);
			UVeyraCombatRollSubsystem* Rolls = Spawner.GetWorld().GetSubsystem<UVeyraCombatRollSubsystem>();
			ASSERT_THAT(IsNotNull(Rolls));
			const FVeyraContentId Attacks = VeyraCrit::BasicAttackChannel();
			const FVeyraContentId Ability = FVeyraContentId::FromText(TEXT("test_strike")).GetValue();
			constexpr int32 Checks = 25;
			const auto AbilityDraws = [&](int32 AttacksBetween) {
				Rolls->SetSeed(Seed);
				UVeyraCombatRollSubsystem::Draw(&Spawner.GetWorld(), Unit, Attacks, Draws);
				TArray<double> Values;
				for (int32 Draw = 0; Draw < Checks; ++Draw)
				{
					Values.Add(UVeyraCombatRollSubsystem::Draw(&Spawner.GetWorld(), Unit, Ability, Draws));
					for (int32 Between = 0; Between < AttacksBetween; ++Between)
					{
						UVeyraCombatRollSubsystem::Draw(&Spawner.GetWorld(), Unit, Attacks, Draws);
						UVeyraCombatRollSubsystem::Draw(&Spawner.GetWorld(), Other, Attacks, Draws);
					}
				}
				return Values;
			};
			ASSERT_THAT(IsTrue(AbilityDraws(0) == AbilityDraws(3), TEXT("basic attacks, the unit's or another's, never change the ability's draws")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
