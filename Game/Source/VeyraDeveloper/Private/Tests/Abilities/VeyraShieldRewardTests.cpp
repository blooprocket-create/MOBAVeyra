// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Delivery/VeyraShieldRewardSubsystem.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	// Veyra.Abilities.ShieldRewards.*: a shield that rewards its holder once it has absorbed a share of
	// what it granted (ADR-027 §5), as Windward's second speed burst.
	TEST_CLASS(ShieldRewards, "Veyra.Abilities")
	{
		// Fixture values, independent of any Vanguard's data.
		static constexpr double ShieldAmount = 100.0;
		static constexpr double Share = 0.5;
		static constexpr double Glancing = 40.0;
		static constexpr double LongSeconds = 60.0;

		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Holder = nullptr;
		AVeyraVanguardCharacter* Enemy = nullptr;

		BEFORE_EACH()
		{
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_second_wind")), StatusOf(EVeyraStatusKind::MoveSpeed, 0.2, LongSeconds));
			FVeyraSelfBuffAbilityTuning Windward;
			Windward.Cast = InstantCast(0.0, LongSeconds, 0.0);
			FVeyraShieldTuning& Shield = Windward.Shields.AddDefaulted_GetRef();
			Shield.Id = ArchetypeTestId(TEXT("test_windward_shield"));
			Shield.Category = EVeyraShieldCategory::Universal;
			Shield.AmountByRank = { ShieldAmount };
			Shield.DurationSeconds = LongSeconds;
			Shield.AbsorbedReward.Add(FVeyraAbsorbedRewardTuning{ Share, { ArchetypeTestId(TEXT("test_second_wind")) } });
			Tuning.SelfBuff.Add(ArchetypeTestId(TEXT("test_windward")), Windward);
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);

			FArchetypeTestWorld World{ Spawner };
			Holder = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			Enemy = &World.Spawn(EVeyraTeam::B, FVector(500.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(World.Learn(*Holder, EVeyraAbilitySlot::E, ArchetypeTestId(TEXT("test_windward")))));
			ASSERT_THAT(IsTrue(VeyraAbilities::TryCast(*Holder->GetAbilitySystemComponent(), EVeyraAbilitySlot::E, FVeyraCastTarget()) == EVeyraCastRejection::None));
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		void Hit(double Amount) const
		{
			FVeyraRawDamageEvent Blow;
			Blow.Components.Add({ EVeyraDamageType::TrueDamage, Amount });
			VeyraCombat::DealDamage(*Enemy->GetAbilitySystemComponent(), *Holder->GetAbilitySystemComponent(), Blow);
		}

		const UVeyraShieldRewardSubsystem& Rewards()
		{
			return *Spawner.GetWorld().GetSubsystem<UVeyraShieldRewardSubsystem>();
		}

		TEST_METHOD(TheRewardComesOnceTheShieldHasAbsorbedItsShare)
		{
			FVeyraContentId Absorbing;
			Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>()->OnDamageResolved.AddLambda([&Absorbing](const FVeyraDamageResolution& Resolution) {
				Absorbing = Resolution.Shields.IsEmpty() ? Absorbing : Resolution.Shields[0].Id;
			});
			Hit(Glancing);
			ASSERT_THAT(IsFalse(FArchetypeTestWorld::Has(*Holder, TEXT("test_second_wind")), TEXT("not yet half")));
			ASSERT_THAT(IsTrue(Absorbing == ArchetypeTestId(TEXT("test_windward_shield")), TEXT("Combat names the shield that absorbed it")));
			Hit(ShieldAmount * Share - Glancing);
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(*Holder, TEXT("test_second_wind")), TEXT("half absorbed: the reward")));
			ASSERT_THAT(AreEqual(0, Rewards().GetWatchCount(), TEXT("once only")));
		}

		TEST_METHOD(AShieldThatAbsorbsTooLittleGivesNothing)
		{
			Hit(Glancing);
			ASSERT_THAT(IsFalse(FArchetypeTestWorld::Has(*Holder, TEXT("test_second_wind"))));
			ASSERT_THAT(AreEqual(1, Rewards().GetWatchCount(), TEXT("it still watches")));
		}

		TEST_METHOD(ValidationKeepsTheRewardInShape)
		{
			constexpr int32 RankCounts[] = { 5, 3 };
			ASSERT_THAT(IsTrue(VeyraAbilityRules::Validate(Tuning, RankCounts).IsEmpty(), FString::Join(VeyraAbilityRules::Validate(Tuning, RankCounts), TEXT(" | "))));
			FVeyraAbilitiesTuning Broken = Tuning;
			Broken.SelfBuff.FindChecked(ArchetypeTestId(TEXT("test_windward"))).Shields[0].AbsorbedReward[0].Fraction = 0.0;
			const FString All = FString::Join(VeyraAbilityRules::Validate(Broken, RankCounts), TEXT(" | "));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/selfBuff/test_windward/shields/0/absorbedReward/0:")), All));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
