// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attacks/VeyraBasicAttackComponent.h"
#include "Attributes/VeyraResourceSet.h"
#include "CQTest.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	/** Fixture values for a cast that costs a share of what its caster holds, and attacks a status lasts. */
	namespace CostShareFixture
	{
		constexpr double Share = 0.5;
		constexpr double Least = 20.0;
		constexpr double Full = 100.0;
		constexpr double Scant = 10.0;
		constexpr double Reduction = 0.5;
		constexpr double Quickening = 0.5;
		constexpr double Lasting = 6.0;
		constexpr int32 Charges = 2;
		constexpr double Near = 100.0;
		constexpr double Windup = 0.25;
		constexpr double Tolerance = 1e-3;
		constexpr float Step = 0.1f;
	}

	// Veyra.Abilities.CostShares.*: a cast that costs a share of its caster's current resource and needs a
	// least of it; cost reduction; a status its holder's attacks spend (ADR-033 §3, §4).
	TEST_CLASS(CostShares, "Veyra.Abilities")
	{
		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Caster = nullptr;

		BEFORE_EACH()
		{
			using namespace CostShareFixture;
			FVeyraSelfBuffAbilityTuning Discharge;
			Discharge.Cast = InstantCast(0.0, 0.0, 0.0);
			Discharge.Cast.CurrentResourceFraction = { Share };
			Discharge.Cast.MinimumResource = { Least };
			Discharge.Statuses = { ArchetypeTestId(TEXT("test_overclocked")) };
			Tuning.SelfBuff.Add(ArchetypeTestId(TEXT("test_discharge")), Discharge);
			FVeyraStatusTuning Overclocked = StatusOf(EVeyraStatusKind::AttackSpeed, Quickening, Lasting);
			Overclocked.AttackCharges = Charges;
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_overclocked")), Overclocked);
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_grid_discount")), StatusOf(EVeyraStatusKind::ResourceCostReduction, Reduction, Lasting));
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);
			const int32 Ranks[] = { 5, 3 };
			ASSERT_THAT(IsTrue(VeyraAbilityRules::Validate(Tuning, Ranks).IsEmpty(), FString::Join(VeyraAbilityRules::Validate(Tuning, Ranks), TEXT(" | "))));

			FArchetypeTestWorld World{ Spawner };
			Caster = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Caster, EVeyraAbilitySlot::Q, ArchetypeTestId(TEXT("test_discharge")))));
			UAbilitySystemComponent& Abilities = *Caster->GetAbilitySystemComponent();
			ASSERT_THAT(IsTrue(VeyraCombat::KeepResource(Abilities) && VeyraCombat::RestoreResource(Abilities, Full)));
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		/** Lets at least Seconds of world time pass, so the next attack may start; no timer ticks. */
		void Wait(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, CostShareFixture::Step);
			}
		}

		double Held() const
		{
			return Caster->GetAbilitySystemComponent()->GetNumericAttribute(UVeyraResourceSet::GetResourceAttribute());
		}

		EVeyraCastRejection Discharge() const
		{
			return FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::Q, Caster->GetActorLocation());
		}

		TEST_METHOD(ItCostsAShareOfWhatItsCasterHoldsAndNeedsALeast)
		{
			using namespace CostShareFixture;
			ASSERT_THAT(IsTrue(Discharge() == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Held(), Full * (1.0 - Share), Tolerance), TEXT("half of what it held")));
			ASSERT_THAT(IsTrue(Discharge() == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Held(), Full * (1.0 - Share) * (1.0 - Share), Tolerance), TEXT("half of what was left")));
			ASSERT_THAT(IsTrue(VeyraCombat::SpendResource(*Caster->GetAbilitySystemComponent(), Held() - Scant)));
			ASSERT_THAT(IsTrue(Discharge() == EVeyraCastRejection::InsufficientResource, TEXT("below its least, refused")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Held(), Scant, Tolerance), TEXT("and nothing spent")));
		}

		TEST_METHOD(ACostReductionLeavesItsShareOfTheCost)
		{
			using namespace CostShareFixture;
			UAbilitySystemComponent& Abilities = *Caster->GetAbilitySystemComponent();
			VeyraCombat::ApplyStatus(Abilities, Abilities, UVeyraAbilitiesTuningSubsystem::FindStatus(ArchetypeTestId(TEXT("test_grid_discount"))).GetValue());
			ASSERT_THAT(IsTrue(Discharge() == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Held(), Full * (1.0 - Share * (1.0 - Reduction)), Tolerance)));
		}

		TEST_METHOD(AStatusAttacksSpendEndsWithItsLastAttack)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(CostShareFixture::Near, 0.0, 0.0));
			ASSERT_THAT(IsTrue(Discharge() == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(*Caster, TEXT("test_overclocked"))));
			FVeyraBasicAttackProfile Profile;
			Profile.Range = CostShareFixture::Near * 2.0;
			Profile.DamageType = EVeyraDamageType::TrueDamage;
			Profile.PhysicalPowerRatio = 1.0;
			Profile.WindupFraction = CostShareFixture::Windup;
			Profile.AcquisitionRadius = Profile.Range;
			UVeyraBasicAttackComponent* Attacks = Caster->GetPlayerState()->FindComponentByClass<UVeyraBasicAttackComponent>();
			ASSERT_THAT(IsTrue(Attacks && Attacks->SetProfile(Profile)));
			for (int32 Attack = 1; Attack <= CostShareFixture::Charges; ++Attack)
			{
				ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(*Caster, TEXT("test_overclocked")), *FString::Printf(TEXT("before attack %d"), Attack)));
				ASSERT_THAT(IsTrue(Attacks->StartAttack(Enemy) == EVeyraAttackRejection::None));
				Attacks->Commit();
				Wait(Attacks->GetTiming().IntervalSeconds);
			}
			ASSERT_THAT(IsFalse(FArchetypeTestWorld::Has(*Caster, TEXT("test_overclocked")), TEXT("its last attack ends it")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
