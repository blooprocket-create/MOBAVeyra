// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attributes/VeyraOffenceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "CQTest.h"
#include "Engine/Engine.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Progression/VeyraProgressionRules.h"
#include "Progression/VeyraProgressionTuning.h"
#include "Progression/VeyraProgressionTuningSubsystem.h"
#include "Tests/Combat/VeyraCombatTestHelpers.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraEconomyTests
{
	/**
	 * A short curve, so the rules are checked independently of the committed tuning: five levels, basic
	 * abilities to rank 3, and an ultimate with two ranks opening at levels 3 and 5. Test fixture values.
	 */
	inline FVeyraProgressionTuning TestProgressionTuning()
	{
		FVeyraProgressionTuning Tuning;
		Tuning.MaxLevel = 5;
		Tuning.SkillPointsPerLevel = 1;
		Tuning.BasicAbilityMaxRank = 3;
		Tuning.UltimateMaxRank = 2;
		Tuning.UltimateRankLevels = { 3, 5 };
		Tuning.Experience.ToNextLevel = { 100, 200, 300, 400 };
		return Tuning;
	}

	// Veyra.Economy.Progression.*: levels, XP, skill points and rank gates as pure rules (Economy &
	// Progression Bible §1, §9).
	TEST_CLASS(Progression, "Veyra.Economy")
	{
		const FVeyraProgressionTuning Tuning = TestProgressionTuning();

		TEST_METHOD(ExperienceCarriesThroughSeveralLevels)
		{
			int32 Gained = 0;
			const VeyraProgression::FExperienceState After = VeyraProgression::AddExperience({ 1, 0.0 }, 350.0, Tuning, Gained);
			ASSERT_THAT(AreEqual(3, After.Level));
			ASSERT_THAT(IsTrue(After.Experience == 350.0 - 100.0 - 200.0));
			ASSERT_THAT(AreEqual(2, Gained));
		}

		TEST_METHOD(ExperienceKeepsItsFraction)
		{
			// Economy & Progression Bible §1: shared rewards keep full precision.
			int32 Gained = 0;
			const VeyraProgression::FExperienceState After = VeyraProgression::AddExperience({ 1, 0.0 }, 100.5, Tuning, Gained);
			ASSERT_THAT(IsTrue(After.Level == 2 && FMath::IsNearlyEqual(After.Experience, 0.5)));
		}

		TEST_METHOD(ExperiencePastTheCapIsDiscarded)
		{
			int32 Gained = 0;
			const VeyraProgression::FExperienceState After = VeyraProgression::AddExperience({ 4, 50.0 }, 100000.0, Tuning, Gained);
			ASSERT_THAT(AreEqual(Tuning.MaxLevel, After.Level));
			ASSERT_THAT(IsTrue(After.Experience == 0.0));
			ASSERT_THAT(AreEqual(1, Gained));
			ASSERT_THAT(AreEqual(0, VeyraProgression::ExperienceToNextLevel(Tuning.MaxLevel, Tuning)));
		}

		TEST_METHOD(OneSkillPointPerLevelFromLevelOne)
		{
			ASSERT_THAT(AreEqual(1, VeyraProgression::SkillPointsEarned(1, Tuning)));
			ASSERT_THAT(AreEqual(Tuning.MaxLevel, VeyraProgression::SkillPointsEarned(Tuning.MaxLevel, Tuning)));
		}

		TEST_METHOD(TheUltimateOpensAtItsLevelsAndBasicsHaveNoGate)
		{
			ASSERT_THAT(AreEqual(0, VeyraProgression::MaxRankAtLevel(EVeyraAbilitySlot::R, 2, Tuning)));
			ASSERT_THAT(AreEqual(1, VeyraProgression::MaxRankAtLevel(EVeyraAbilitySlot::R, 3, Tuning)));
			ASSERT_THAT(AreEqual(1, VeyraProgression::MaxRankAtLevel(EVeyraAbilitySlot::R, 4, Tuning)));
			ASSERT_THAT(AreEqual(2, VeyraProgression::MaxRankAtLevel(EVeyraAbilitySlot::R, 5, Tuning)));
			ASSERT_THAT(AreEqual(Tuning.BasicAbilityMaxRank, VeyraProgression::MaxRankAtLevel(EVeyraAbilitySlot::Q, 1, Tuning)));
		}

		TEST_METHOD(RankUpsAreRefusedForEachReason)
		{
			ASSERT_THAT(IsTrue(VeyraProgression::CheckRankUp(EVeyraAbilitySlot::Q, 3, 5, 1, Tuning) == EVeyraRankRefusal::MaxRank));
			ASSERT_THAT(IsTrue(VeyraProgression::CheckRankUp(EVeyraAbilitySlot::R, 0, 2, 1, Tuning) == EVeyraRankRefusal::LevelTooLow));
			ASSERT_THAT(IsTrue(VeyraProgression::CheckRankUp(EVeyraAbilitySlot::R, 1, 4, 1, Tuning) == EVeyraRankRefusal::LevelTooLow));
			ASSERT_THAT(IsTrue(VeyraProgression::CheckRankUp(EVeyraAbilitySlot::Q, 0, 1, 0, Tuning) == EVeyraRankRefusal::NoSkillPoint));
			ASSERT_THAT(IsTrue(VeyraProgression::CheckRankUp(EVeyraAbilitySlot::Q, 0, 1, 1, Tuning) == EVeyraRankRefusal::None));
		}

		TEST_METHOD(ValidationCatchesAnInconsistentCurveAndRanks)
		{
			FVeyraProgressionTuning Broken = TestProgressionTuning();
			Broken.Experience.ToNextLevel.Pop();
			Broken.UltimateRankLevels = { 5, 3 };
			const TArray<FString> Problems = VeyraProgression::Validate(Broken);
			ASSERT_THAT(IsTrue(Problems.ContainsByPredicate([](const FString& Problem) { return Problem.StartsWith(TEXT("/experience/toNextLevel:")); }),
				FString::Join(Problems, TEXT(" | "))));
			ASSERT_THAT(IsTrue(Problems.ContainsByPredicate([](const FString& Problem) { return Problem.StartsWith(TEXT("/ultimateRankLevels/1:")); }),
				FString::Join(Problems, TEXT(" | "))));
			ASSERT_THAT(IsTrue(VeyraProgression::Validate(TestProgressionTuning()).IsEmpty()));
		}

		TEST_METHOD(TheCommittedFileLoads)
		{
			const UVeyraProgressionTuningSubsystem* Subsystem = GEngine->GetEngineSubsystem<UVeyraProgressionTuningSubsystem>();
			ASSERT_THAT(IsTrue(Subsystem && Subsystem->IsLoaded()));
			ASSERT_THAT(IsTrue(VeyraProgression::Validate(UVeyraProgressionTuningSubsystem::Get()).IsEmpty()));
		}
	};

	// Veyra.Economy.ProgressionComponent.*: a participant's progression, applied to its stats (Economy &
	// Progression Bible §9).
	TEST_CLASS(ProgressionComponent, "Veyra.Economy")
	{
		FVeyraProgressionTuning Tuning = TestProgressionTuning();
		FActorTestSpawner Spawner;
		UAbilitySystemComponent* Unit = nullptr;
		UVeyraProgressionComponent* Progression = nullptr;

		BEFORE_EACH()
		{
			UVeyraProgressionTuningSubsystem::SetTestOverride(&Tuning);
			Unit = &VeyraCombatTests::SpawnCombatant(Spawner);
			Progression = Unit->GetOwner()->FindComponentByClass<UVeyraProgressionComponent>();
			ASSERT_THAT(IsNotNull(Progression));
		}

		AFTER_EACH()
		{
			UVeyraProgressionTuningSubsystem::SetTestOverride(nullptr);
		}

		TEST_METHOD(StartsAtLevelOneWithOnePointAndNoRanks)
		{
			ASSERT_THAT(IsFalse(Progression->IsInitialized()));
			Progression->Initialize(FVeyraStatGrowth(), 0.0);
			ASSERT_THAT(AreEqual(1, Progression->GetLevel()));
			ASSERT_THAT(AreEqual(VeyraProgression::SkillPointsEarned(1, Tuning), Progression->GetUnspentSkillPoints()));
			for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::All)
			{
				ASSERT_THAT(AreEqual(0, Progression->GetRank(Slot)));
			}
		}

		TEST_METHOD(LevellingUpGrowsTheStatsAndGrantsPoints)
		{
			constexpr double MaxHealth = 600.0;
			constexpr double HealthLost = 100.0;
			constexpr double BaseAttackSpeed = 0.625;
			FVeyraStatBlock Stats;
			Stats.MaxHealth = MaxHealth;
			Stats.AttackSpeed = BaseAttackSpeed;
			Stats.MoveSpeed = 335.0;
			ASSERT_THAT(IsTrue(VeyraCombat::InitializeStats(*Unit, Stats)));
			Unit->SetNumericAttributeBase(UVeyraVitalsSet::GetHealthAttribute(), static_cast<float>(MaxHealth - HealthLost));

			FVeyraStatGrowth Growth;
			Growth.MaxHealth = 50.0;
			Growth.AttackSpeedFraction = 0.1;
			Progression->Initialize(Growth, BaseAttackSpeed);
			int32 Announced = 0;
			Progression->OnLevelGained.AddLambda([&Announced](int32) { ++Announced; });

			const int32 Gained = Progression->AddExperience(Tuning.Experience.ToNextLevel[0] + Tuning.Experience.ToNextLevel[1]);
			ASSERT_THAT(AreEqual(2, Gained));
			ASSERT_THAT(AreEqual(2, Announced));
			ASSERT_THAT(AreEqual(3, Progression->GetLevel()));
			ASSERT_THAT(AreEqual(VeyraProgression::SkillPointsEarned(3, Tuning), Progression->GetUnspentSkillPoints()));
			ASSERT_THAT(IsTrue(Unit->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()) == MaxHealth + 2.0 * Growth.MaxHealth));
			ASSERT_THAT(IsTrue(Unit->GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute()) == MaxHealth + 2.0 * Growth.MaxHealth - HealthLost));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Unit->GetNumericAttribute(UVeyraOffenceSet::GetAttackSpeedAttribute()),
				BaseAttackSpeed * (1.0 + 2.0 * Growth.AttackSpeedFraction), 1e-5)));
		}

		TEST_METHOD(AllocatingSpendsAPointAndRespectsTheGates)
		{
			Progression->Initialize(FVeyraStatGrowth(), 0.0);
			ASSERT_THAT(IsTrue(Progression->AllocateRank(EVeyraAbilitySlot::R) == EVeyraRankRefusal::LevelTooLow));
			ASSERT_THAT(IsTrue(Progression->AllocateRank(EVeyraAbilitySlot::Q) == EVeyraRankRefusal::None));
			ASSERT_THAT(AreEqual(1, Progression->GetRank(EVeyraAbilitySlot::Q)));
			ASSERT_THAT(AreEqual(0, Progression->GetUnspentSkillPoints()));
			ASSERT_THAT(IsTrue(Progression->AllocateRank(EVeyraAbilitySlot::W) == EVeyraRankRefusal::NoSkillPoint));
		}

		TEST_METHOD(ItemSlotsTakeNoRanks)
		{
			// An item's Active has no ranks (ADR-012 §1); a request for one changes nothing.
			Progression->Initialize(FVeyraStatGrowth(), 0.0);
			ASSERT_THAT(IsTrue(Progression->AllocateRank(EVeyraAbilitySlot::Item1) == EVeyraRankRefusal::MaxRank));
			ASSERT_THAT(AreEqual(VeyraProgression::SkillPointsEarned(1, Tuning), Progression->GetUnspentSkillPoints()));
			ASSERT_THAT(AreEqual(0, Progression->GetRank(EVeyraAbilitySlot::Item6)));
		}

		TEST_METHOD(NothingProgressesBeforeInitialization)
		{
			ASSERT_THAT(AreEqual(0, Progression->AddExperience(1000)));
			ASSERT_THAT(IsTrue(Progression->AllocateRank(EVeyraAbilitySlot::Q) == EVeyraRankRefusal::NotInitialized));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
