// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attacks/VeyraBasicAttackComponent.h"
#include "Attributes/VeyraResourceSet.h"
#include "CQTest.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	/** Fixture values for grids: an aura on Fluxborn, an attack's cooldown cut and a draining buff. */
	namespace GridFixture
	{
		constexpr double Radius = 500.0;
		constexpr double Near = 150.0;
		constexpr double Lasting = 4.0;
		constexpr double Refresh = 0.5;
		constexpr double Overclock = 0.3;
		constexpr double Cut = 1.0;
		constexpr double LongCooldown = 10.0;
		constexpr double ShortCooldown = 5.0;
		constexpr double PerSecond = 20.0;
		constexpr double Interval = 0.5;
		constexpr double MaxSeconds = 8.0;
		constexpr double BriefMax = 1.0;
		constexpr double Little = 20.0;
		constexpr double Plenty = 300.0;
		constexpr double ManyLevels = 5000.0;
		constexpr double Windup = 0.25;
		constexpr double Tolerance = 0.05;
		constexpr float Step = 0.1f;
	}

	// Veyra.Abilities.Grids.*: auras on allied Fluxborn, attacks that cut the next basic ability's cooldown,
	// and buffs that drain their caster's resource (ADR-033 §6).
	TEST_CLASS(Grids, "Veyra.Abilities")
	{
		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Caster = nullptr;

		BEFORE_EACH()
		{
			using namespace GridFixture;
			FVeyraSelfBuffAbilityTuning Grid;
			Grid.Cast = InstantCast(0.0, 0.0, 0.0);
			FVeyraAuraTuning& Aura = Grid.Aura.AddDefaulted_GetRef();
			Aura.Radius = Radius;
			Aura.DurationSeconds = Lasting;
			Aura.RefreshSeconds = Refresh;
			Aura.AllyFluxbornStatuses = { ArchetypeTestId(TEXT("test_fluxborn_overclock")) };
			Tuning.SelfBuff.Add(ArchetypeTestId(TEXT("test_grid")), Grid);

			FVeyraSelfBuffAbilityTuning Slow;
			Slow.Cast = InstantCast(0.0, LongCooldown, 0.0);
			Tuning.SelfBuff.Add(ArchetypeTestId(TEXT("test_slow_spell")), Slow);
			FVeyraSelfBuffAbilityTuning Quick;
			Quick.Cast = InstantCast(0.0, ShortCooldown, 0.0);
			Tuning.SelfBuff.Add(ArchetypeTestId(TEXT("test_quick_spell")), Quick);

			FVeyraSelfBuffAbilityTuning Drained;
			Drained.Cast = InstantCast(0.0, 0.0, 0.0);
			Drained.Statuses = { ArchetypeTestId(TEXT("test_anchored")) };
			Drained.Drain.Add(FVeyraDrainTuning{ PerSecond, Interval, MaxSeconds });
			Tuning.SelfBuff.Add(ArchetypeTestId(TEXT("test_drained")), Drained);
			FVeyraSelfBuffAbilityTuning Brief = Drained;
			Brief.Drain[0].MaxSeconds = BriefMax;
			Tuning.SelfBuff.Add(ArchetypeTestId(TEXT("test_brief")), Brief);

			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_fluxborn_overclock")), StatusOf(EVeyraStatusKind::AttackSpeed, Overclock, Refresh * 2.0));
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_link")), StatusOf(EVeyraStatusKind::AttackShortensCooldown, Cut, LongCooldown));
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_anchored")), StatusOf(EVeyraStatusKind::Planted, 0.0, LongCooldown));
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);
			const int32 Ranks[] = { 5, 3 };
			ASSERT_THAT(IsTrue(VeyraAbilityRules::Validate(Tuning, Ranks).IsEmpty(), FString::Join(VeyraAbilityRules::Validate(Tuning, Ranks), TEXT(" | "))));

			FArchetypeTestWorld World{ Spawner };
			Caster = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		/** Grants each ability in its slot and ranks each once. */
		void LearnAll(TConstArrayView<TPair<EVeyraAbilitySlot, const TCHAR*>> Kit)
		{
			APlayerState* PlayerState = Caster->GetPlayerState();
			UVeyraProgressionComponent* Progression = PlayerState->FindComponentByClass<UVeyraProgressionComponent>();
			Progression->Initialize(FVeyraStatGrowth(), 0.0);
			Progression->AddExperience(GridFixture::ManyLevels);
			UVeyraAbilityLoadoutComponent* Loadout = PlayerState->FindComponentByClass<UVeyraAbilityLoadoutComponent>();
			for (const TPair<EVeyraAbilitySlot, const TCHAR*>& Entry : Kit)
			{
				ASSERT_THAT(IsTrue(Loadout->Grant(*Caster->GetAbilitySystemComponent(), Entry.Key, ArchetypeTestId(Entry.Value))));
				ASSERT_THAT(IsTrue(Progression->AllocateRank(Entry.Key) == EVeyraRankRefusal::None));
			}
		}

		EVeyraCastRejection CastIn(EVeyraAbilitySlot Slot) const
		{
			return FArchetypeTestWorld::CastAt(*Caster, Slot, Caster->GetActorLocation());
		}

		void Wait(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, GridFixture::Step);
				++GFrameCounter;
				World.GetTimerManager().Tick(GridFixture::Step);
			}
		}

		double Held() const
		{
			return Caster->GetAbilitySystemComponent()->GetNumericAttribute(UVeyraResourceSet::GetResourceAttribute());
		}

		TEST_METHOD(AnAuraOverclocksAlliedFluxbornAndNoOthers)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraTestFluxborn& Ally = World.SpawnFluxborn(EVeyraTeam::A, FVector(GridFixture::Near, 0.0, 0.0));
			AVeyraTestFluxborn& Enemy = World.SpawnFluxborn(EVeyraTeam::B, FVector(-GridFixture::Near, 0.0, 0.0));
			LearnAll({ { EVeyraAbilitySlot::R, TEXT("test_grid") } });
			ASSERT_THAT(IsTrue(CastIn(EVeyraAbilitySlot::R) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(Ally, TEXT("test_fluxborn_overclock")), TEXT("an allied Fluxborn is overclocked")));
			ASSERT_THAT(IsFalse(FArchetypeTestWorld::Has(Enemy, TEXT("test_fluxborn_overclock")), TEXT("an enemy one is not")));
		}

		TEST_METHOD(AnAttackCutsTheCooldownThatEndsSoonest)
		{
			using namespace GridFixture;
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			LearnAll({ { EVeyraAbilitySlot::Q, TEXT("test_slow_spell") }, { EVeyraAbilitySlot::W, TEXT("test_quick_spell") } });
			ASSERT_THAT(IsTrue(CastIn(EVeyraAbilitySlot::Q) == EVeyraCastRejection::None && CastIn(EVeyraAbilitySlot::W) == EVeyraCastRejection::None));
			UAbilitySystemComponent& Abilities = *Caster->GetAbilitySystemComponent();
			VeyraCombat::ApplyStatus(Abilities, Abilities, UVeyraAbilitiesTuningSubsystem::FindStatus(ArchetypeTestId(TEXT("test_link"))).GetValue());
			const UVeyraCooldownComponent& Cooldowns = *Caster->GetPlayerState()->FindComponentByClass<UVeyraCooldownComponent>();
			const double SlowBefore = Cooldowns.GetRemainingSecondsNow(ArchetypeTestId(TEXT("test_slow_spell")));
			const double QuickBefore = Cooldowns.GetRemainingSecondsNow(ArchetypeTestId(TEXT("test_quick_spell")));
			FVeyraBasicAttackProfile Profile;
			Profile.Range = Near * 2.0;
			Profile.DamageType = EVeyraDamageType::TrueDamage;
			Profile.PhysicalPowerRatio = 1.0;
			Profile.WindupFraction = Windup;
			Profile.AcquisitionRadius = Profile.Range;
			UVeyraBasicAttackComponent* Attacks = Caster->GetPlayerState()->FindComponentByClass<UVeyraBasicAttackComponent>();
			ASSERT_THAT(IsTrue(Attacks && Attacks->SetProfile(Profile) && Attacks->StartAttack(Enemy) == EVeyraAttackRejection::None));
			Attacks->Commit();
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Cooldowns.GetRemainingSecondsNow(ArchetypeTestId(TEXT("test_quick_spell"))), QuickBefore - Cut, Tolerance),
				TEXT("the soonest loses the cut")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Cooldowns.GetRemainingSecondsNow(ArchetypeTestId(TEXT("test_slow_spell"))), SlowBefore, Tolerance), TEXT("the other keeps its time")));
		}

		TEST_METHOD(ADrainingBuffEndsAsItsResourceRunsOut)
		{
			using namespace GridFixture;
			LearnAll({ { EVeyraAbilitySlot::R, TEXT("test_drained") } });
			UAbilitySystemComponent& Abilities = *Caster->GetAbilitySystemComponent();
			ASSERT_THAT(IsTrue(VeyraCombat::KeepResource(Abilities) && VeyraCombat::RestoreResource(Abilities, Little)));
			ASSERT_THAT(IsTrue(CastIn(EVeyraAbilitySlot::R) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(*Caster, TEXT("test_anchored"))));
			Wait(Little / PerSecond + Interval);
			ASSERT_THAT(IsTrue(FMath::IsNearlyZero(Held()), TEXT("drained")));
			ASSERT_THAT(IsFalse(FArchetypeTestWorld::Has(*Caster, TEXT("test_anchored")), TEXT("and the buff ended with it")));
		}

		TEST_METHOD(ADrainingBuffEndsAtItsLongestToo)
		{
			using namespace GridFixture;
			LearnAll({ { EVeyraAbilitySlot::R, TEXT("test_brief") } });
			UAbilitySystemComponent& Abilities = *Caster->GetAbilitySystemComponent();
			ASSERT_THAT(IsTrue(VeyraCombat::KeepResource(Abilities) && VeyraCombat::RestoreResource(Abilities, Plenty)));
			ASSERT_THAT(IsTrue(CastIn(EVeyraAbilitySlot::R) == EVeyraCastRejection::None));
			Wait(BriefMax + Interval);
			ASSERT_THAT(IsTrue(Held() > 0.0, TEXT("resource left")));
			ASSERT_THAT(IsFalse(FArchetypeTestWorld::Has(*Caster, TEXT("test_anchored")), TEXT("but its time is up")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
