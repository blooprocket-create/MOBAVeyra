// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Delivery/VeyraShieldHoldSubsystem.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	/** Fixture values for a shell whose shield holds a status and bursts as it ends. */
	namespace ShieldHoldFixture
	{
		constexpr double Amount = 100.0;
		constexpr double ShieldSeconds = 3.0;
		constexpr double StatusSeconds = 10.0;
		constexpr double Steadiness = 0.3;
		constexpr double Burst = 300.0;
		constexpr double Blow = 40.0;
		constexpr double Near = 200.0;
		constexpr double Breaking = 150.0;
		constexpr float Step = 0.1f;
	}

	// Veyra.Abilities.ShieldHolds.*: the statuses a self-buff's shield holds, and the burst as it breaks
	// or runs out (ADR-032 §3).
	TEST_CLASS(ShieldHolds, "Veyra.Abilities")
	{
		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Caster = nullptr;
		AVeyraVanguardCharacter* Enemy = nullptr;

		BEFORE_EACH()
		{
			using namespace ShieldHoldFixture;
			FVeyraSelfBuffAbilityTuning Shell;
			Shell.Cast = InstantCast(0.0, 0.0, 0.0);
			FVeyraShieldTuning& Shield = Shell.Shields.AddDefaulted_GetRef();
			Shield.Id = ArchetypeTestId(TEXT("test_shell_shield"));
			Shield.Category = EVeyraShieldCategory::Universal;
			Shield.AmountByRank = { Amount };
			Shield.DurationSeconds = ShieldSeconds;
			Shell.ShieldHolds = { ArchetypeTestId(TEXT("test_steady")) };
			FVeyraAreaZoneTuning& Zone = Shell.ShieldEndZones.AddDefaulted_GetRef();
			Zone.Shape = CircleOf(Burst);
			FVeyraDamageTuning& Damage = Zone.Effects.Damage.AddDefaulted_GetRef();
			Damage.Type = EVeyraDamageType::TrueDamage;
			Damage.AmountByRank = { Blow };
			Tuning.SelfBuff.Add(ArchetypeTestId(TEXT("test_shell")), Shell);
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_steady")), StatusOf(EVeyraStatusKind::Tenacity, Steadiness, StatusSeconds));
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);
			const int32 Ranks[] = { 5, 3 };
			ASSERT_THAT(IsTrue(VeyraAbilityRules::Validate(Tuning, Ranks).IsEmpty(), FString::Join(VeyraAbilityRules::Validate(Tuning, Ranks), TEXT(" | "))));

			FArchetypeTestWorld World{ Spawner };
			Caster = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			Enemy = &World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Caster, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_shell")))));
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		EVeyraCastRejection Raise() const
		{
			return FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::W, Caster->GetActorLocation());
		}

		void Wait(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, ShieldHoldFixture::Step);
				++GFrameCounter;
				World.GetTimerManager().Tick(ShieldHoldFixture::Step);
			}
		}

		TEST_METHOD(ItsStatusGoesAndItBurstsAsTheShieldBreaks)
		{
			ASSERT_THAT(IsTrue(Raise() == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(*Caster, TEXT("test_steady")), TEXT("held while the shield holds")));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::HealthLost(*Enemy) == 0.0, TEXT("no burst yet")));
			FVeyraRawDamageEvent Hurt;
			Hurt.Components.Add({ EVeyraDamageType::TrueDamage, ShieldHoldFixture::Breaking });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Enemy->GetAbilitySystemComponent(), *Caster->GetAbilitySystemComponent(), Hurt)));
			ASSERT_THAT(IsFalse(FArchetypeTestWorld::Has(*Caster, TEXT("test_steady")), TEXT("gone as it breaks")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FArchetypeTestWorld::HealthLost(*Enemy), ShieldHoldFixture::Blow),
				*FString::Printf(TEXT("and the burst strikes the enemy: lost %g"), FArchetypeTestWorld::HealthLost(*Enemy))));
		}

		TEST_METHOD(ItsStatusGoesAndItBurstsAsTheShieldRunsOut)
		{
			ASSERT_THAT(IsTrue(Raise() == EVeyraCastRejection::None));
			Wait(ShieldHoldFixture::ShieldSeconds + ShieldHoldFixture::Step);
			ASSERT_THAT(IsFalse(FArchetypeTestWorld::Has(*Caster, TEXT("test_steady")), TEXT("it lasts no longer than the shield")));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::HealthLost(*Enemy) > ShieldHoldFixture::Blow / 2.0, TEXT("and the burst strikes the enemy")));
		}

		TEST_METHOD(ANewGrantTakesOverQuietly)
		{
			ASSERT_THAT(IsTrue(Raise() == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Raise() == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(*Caster, TEXT("test_steady")), TEXT("the new shield holds its status")));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::HealthLost(*Enemy) == 0.0, TEXT("the replaced shield did not burst")));
			const UVeyraShieldHoldSubsystem* Holds = Spawner.GetWorld().GetSubsystem<UVeyraShieldHoldSubsystem>();
			ASSERT_THAT(AreEqual(1, Holds->GetHoldCount()));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
