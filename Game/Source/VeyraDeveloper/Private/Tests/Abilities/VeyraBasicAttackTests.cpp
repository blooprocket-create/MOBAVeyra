// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attacks/VeyraBasicAttackComponent.h"
#include "CombatState/VeyraCombatStateComponent.h"
#include "CQTest.h"
#include "Delivery/VeyraProjectile.h"
#include "EngineUtils.h"
#include "Life/VeyraLifeComponent.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	// Veyra.Abilities.BasicAttack.*: windup, Commit and backswing; target checks; empowerment, cleave,
	// secondary impacts and the hit chain (Combat Bible §4, §16, §17, §48; ADR-009 §5). The windup is
	// ended by hand here; Veyra.Net.AttackOrders runs attacks on the server's clock.
	TEST_CLASS(BasicAttack, "Veyra.Abilities")
	{
		// Fixture values, independent of any Vanguard's data.
		static constexpr double Range = 150.0;
		static constexpr double PowerRatio = 1.0;
		static constexpr double WindupFraction = 0.25;
		static constexpr double AcquisitionRadius = 500.0;
		static constexpr double CleaveRadius = 300.0;
		static constexpr double CleaveArc = 120.0;
		static constexpr double CleaveFraction = 0.5;
		static constexpr double BonusDamage = 20.0;
		static constexpr double ImpactRadius = 150.0;
		static constexpr double ImpactDamage = 30.0;
		static constexpr double WeakImpactDamage = 999.0;
		static constexpr double LongSeconds = 60.0;
		static constexpr double Tolerance = 1e-3;

		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Attacker = nullptr;
		UVeyraBasicAttackComponent* Attacks = nullptr;

		static FVeyraBasicAttackProfile Melee()
		{
			FVeyraBasicAttackProfile Profile;
			Profile.Range = Range;
			// True damage, so the tests read exact amounts; the pipeline's mitigation is tested on its own.
			Profile.DamageType = EVeyraDamageType::TrueDamage;
			Profile.PhysicalPowerRatio = PowerRatio;
			Profile.WindupFraction = WindupFraction;
			Profile.AcquisitionRadius = AcquisitionRadius;
			FVeyraShape& Cleave = Profile.Cleave.AddDefaulted_GetRef();
			Cleave.Kind = EVeyraShapeKind::Sector;
			Cleave.Radius = CleaveRadius;
			Cleave.ArcDegrees = CleaveArc;
			return Profile;
		}

		BEFORE_EACH()
		{
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_slow")), StatusOf(EVeyraStatusKind::Slow, 0.3, LongSeconds));
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_cleave")), StatusOf(EVeyraStatusKind::AttackCleave, CleaveFraction, LongSeconds));

			FVeyraEmpoweredAttackAbilityTuning Heavy;
			Heavy.Cast = InstantCast(0.0, LongSeconds, 0.0);
			Heavy.DurationSeconds = LongSeconds;
			Heavy.Damage.Add(FVeyraDamageTuning{ EVeyraDamageType::TrueDamage, { BonusDamage }, 0.0, 0.0 });
			Heavy.Statuses.Add(ArchetypeTestId(TEXT("test_slow")));
			Heavy.ArmorPenetrationByRank = { 0.0 };
			FVeyraSecondaryImpactTuning& Impact = Heavy.SecondaryImpact.AddDefaulted_GetRef();
			Impact.Priority = 2;
			Impact.Shape = CircleOf(ImpactRadius);
			Impact.Damage.Add(FVeyraDamageTuning{ EVeyraDamageType::TrueDamage, { ImpactDamage }, 0.0, 0.0 });
			Tuning.EmpoweredAttack.Add(ArchetypeTestId(TEXT("test_heavy")), Heavy);
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);

			FArchetypeTestWorld World{ Spawner };
			Attacker = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			Attacks = Attacker->GetPlayerState()->FindComponentByClass<UVeyraBasicAttackComponent>();
			ASSERT_THAT(IsTrue(Attacks && Attacks->SetProfile(Melee())));
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		static double BaseDamage()
		{
			return VeyraCombatTests::ExampleStats().PhysicalPower * PowerRatio;
		}

		/** Starts an attack at Target and ends its windup at once. */
		EVeyraAttackRejection AttackNow(AActor& Target) const
		{
			const EVeyraAttackRejection Rejection = Attacks->StartAttack(Target);
			if (Rejection == EVeyraAttackRejection::None)
			{
				Attacks->Commit();
			}
			return Rejection;
		}

		/** Lets at least Seconds of world time pass, as the next attack needs; no timer or actor ticks. */
		void Wait(double Seconds)
		{
			// Fixture value: a step below the longest frame the world accepts in one tick.
			constexpr float StepSeconds = 0.1f;
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, StepSeconds);
			}
		}

		TEST_METHOD(AMeleeAttackWindsUpThenHitsAtCommit)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(100.0, 0.0, 0.0));
			int32 Attacked = 0;
			int32 Hit = 0;
			Attacks->OnAttack.AddLambda([&Attacked](const FVeyraAttackEvent&) { ++Attacked; });
			Attacks->OnHit.AddLambda([&Hit](const FVeyraAttackEvent&) { ++Hit; });

			ASSERT_THAT(IsTrue(Attacks->StartAttack(Enemy) == EVeyraAttackRejection::None));
			ASSERT_THAT(IsTrue(Attacks->GetState().Phase == EVeyraAttackPhase::Windup && Attacks->GetState().Target == &Enemy));
			ASSERT_THAT(IsTrue(World.HealthLost(Enemy) == 0.0 && Attacked == 0, TEXT("nothing lands before Commit")));
			const FVeyraAttackTiming Timing = Attacks->GetTiming();
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Attacks->GetState().PhaseEndsAt, Timing.IntervalSeconds * WindupFraction, Tolerance)));

			Attacks->Commit();
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Enemy), BaseDamage(), Tolerance)));
			ASSERT_THAT(IsTrue(Attacked == 1 && Hit == 1));
			ASSERT_THAT(IsTrue(Attacks->GetState().Phase == EVeyraAttackPhase::Backswing));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Attacks->GetNextAttackAt(), Timing.IntervalSeconds, Tolerance), TEXT("the interval runs from the attack's start")));
			ASSERT_THAT(IsTrue(Attacks->CheckAttack(&Enemy) == EVeyraAttackRejection::OnCooldown));
		}

		TEST_METHOD(TheTargetMustBeInRangeAtTheStartAndAtCommit)
		{
			FArchetypeTestWorld World{ Spawner };
			const FVector Away(Range * 10.0, 0.0, 0.0);
			AVeyraVanguardCharacter& Distant = World.Spawn(EVeyraTeam::B, Away);
			ASSERT_THAT(IsTrue(Attacks->StartAttack(Distant) == EVeyraAttackRejection::OutOfRange));

			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(100.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(Attacks->StartAttack(Enemy) == EVeyraAttackRejection::None));
			Enemy.SetActorLocation(Away + FVector(0.0, Range, 0.0));
			Attacks->Commit();
			ASSERT_THAT(IsTrue(World.HealthLost(Enemy) == 0.0));
			ASSERT_THAT(IsTrue(Attacks->GetState().Phase == EVeyraAttackPhase::None));
			ASSERT_THAT(IsTrue(Attacks->GetNextAttackAt() == 0.0, TEXT("a cancelled attack spends nothing")));
		}

		TEST_METHOD(OnlyALivingEnemyUnitCanBeAttacked)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Friend = World.Spawn(EVeyraTeam::A, FVector(100.0, 0.0, 0.0));
			AVeyraVanguardCharacter& Fallen = World.Spawn(EVeyraTeam::B, FVector(0.0, 100.0, 0.0));
			Fallen.GetPlayerState()->FindComponentByClass<UVeyraLifeComponent>()->SetState(EVeyraLifeState::Dead);
			AVeyraTestFluxborn& Fluxborn = World.SpawnFluxborn(EVeyraTeam::B, FVector(-100.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(Attacks->CheckAttack(&Friend) == EVeyraAttackRejection::InvalidTarget));
			ASSERT_THAT(IsTrue(Attacks->CheckAttack(&Fallen) == EVeyraAttackRejection::InvalidTarget));
			ASSERT_THAT(IsTrue(Attacks->CheckAttack(nullptr) == EVeyraAttackRejection::InvalidTarget));
			ASSERT_THAT(IsTrue(AttackNow(Fluxborn) == EVeyraAttackRejection::None, TEXT("any enemy unit can be attacked")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Fluxborn), BaseDamage(), Tolerance)));
		}

		TEST_METHOD(ActingBeforeCommitCancelsTheAttackAtNoCost)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(100.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(Attacks->StartAttack(Enemy) == EVeyraAttackRejection::None));
			Attacks->CancelAttack();
			Attacks->Commit();
			ASSERT_THAT(IsTrue(World.HealthLost(Enemy) == 0.0 && Attacks->GetState().Phase == EVeyraAttackPhase::None));
			ASSERT_THAT(IsTrue(Attacks->CheckAttack(&Enemy) == EVeyraAttackRejection::None, TEXT("the next attack may start at once")));

			// Casting cancels a windup too (Combat Bible §48).
			ASSERT_THAT(IsTrue(World.Learn(*Attacker, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_heavy")))));
			ASSERT_THAT(IsTrue(Attacks->StartAttack(Enemy) == EVeyraAttackRejection::None));
			ASSERT_THAT(IsTrue(VeyraAbilities::TryCast(*Attacker->GetAbilitySystemComponent(), EVeyraAbilitySlot::W, FVeyraCastTarget()) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Attacks->GetState().Phase == EVeyraAttackPhase::None && World.HealthLost(Enemy) == 0.0));
		}

		TEST_METHOD(AnEmpoweredAttackAddsItsDamageAndStatusesOnce)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(100.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(World.Learn(*Attacker, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_heavy")))));
			ASSERT_THAT(IsTrue(VeyraAbilities::TryCast(*Attacker->GetAbilitySystemComponent(), EVeyraAbilitySlot::W, FVeyraCastTarget()) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Attacks->IsEmpowered()));
			bool bEmpoweredEvent = false;
			Attacks->OnAttack.AddLambda([&bEmpoweredEvent](const FVeyraAttackEvent& Event) { bEmpoweredEvent = Event.bEmpowered; });

			ASSERT_THAT(IsTrue(AttackNow(Enemy) == EVeyraAttackRejection::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Enemy), BaseDamage() + BonusDamage, Tolerance), TEXT("one hit, with the bonus in it")));
			ASSERT_THAT(IsTrue(World.Has(Enemy, TEXT("test_slow")) && bEmpoweredEvent));
			ASSERT_THAT(IsFalse(Attacks->IsEmpowered(), TEXT("the attack consumed it")));
		}

		TEST_METHOD(ACleavingAttackHitsOtherEnemiesForPartOfItsDamage)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Primary = World.Spawn(EVeyraTeam::B, FVector(100.0, 0.0, 0.0));
			AVeyraVanguardCharacter& Beside = World.Spawn(EVeyraTeam::B, FVector(CleaveRadius / 2.0, CleaveRadius / 4.0, 0.0));
			AVeyraVanguardCharacter& Behind = World.Spawn(EVeyraTeam::B, FVector(-CleaveRadius / 2.0, 0.0, 0.0));
			UAbilitySystemComponent& Self = *Attacker->GetAbilitySystemComponent();
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Self, Self, UVeyraAbilitiesTuningSubsystem::FindStatus(ArchetypeTestId(TEXT("test_cleave"))).GetValue())));

			ASSERT_THAT(IsTrue(AttackNow(Primary) == EVeyraAttackRejection::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Primary), BaseDamage(), Tolerance)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Beside), BaseDamage() * CleaveFraction, Tolerance)));
			ASSERT_THAT(IsTrue(World.HealthLost(Behind) == 0.0, TEXT("the cleave sweeps ahead of the attacker")));
		}

		TEST_METHOD(TheHighestPrioritySecondaryImpactLandsBehindTheTarget)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Primary = World.Spawn(EVeyraTeam::B, FVector(100.0, 0.0, 0.0));
			AVeyraVanguardCharacter& BehindIt = World.Spawn(EVeyraTeam::B, FVector(100.0 + ImpactRadius / 2.0, 0.0, 0.0));
			// A lower-priority impact from a modifier, as a passive's would be.
			Attacks->OnModifyAttack.AddLambda([](FVeyraAttackPlan& Plan) {
				FVeyraSecondaryImpact Weak;
				Weak.Priority = 1;
				Weak.Shape = CircleOf(ImpactRadius);
				Weak.Damage.Components.Add({ EVeyraDamageType::TrueDamage, WeakImpactDamage });
				Plan.OfferSecondaryImpact(Weak);
			});
			ASSERT_THAT(IsTrue(World.Learn(*Attacker, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_heavy")))));
			ASSERT_THAT(IsTrue(VeyraAbilities::TryCast(*Attacker->GetAbilitySystemComponent(), EVeyraAbilitySlot::W, FVeyraCastTarget()) == EVeyraCastRejection::None));

			ASSERT_THAT(IsTrue(AttackNow(Primary) == EVeyraAttackRejection::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(BehindIt), ImpactDamage, Tolerance), TEXT("one impact, the higher priority's")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Primary), BaseDamage() + BonusDamage, Tolerance), TEXT("the impact spares its target")));
		}

		TEST_METHOD(TheChainCountsConsecutiveAttacksOnOneEnemyVanguard)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& First = World.Spawn(EVeyraTeam::B, FVector(100.0, 0.0, 0.0));
			AVeyraVanguardCharacter& Second = World.Spawn(EVeyraTeam::B, FVector(0.0, 100.0, 0.0));
			AVeyraTestFluxborn& Fluxborn = World.SpawnFluxborn(EVeyraTeam::B, FVector(-100.0, 0.0, 0.0));
			const double Interval = Attacks->GetTiming().IntervalSeconds;
			const auto ExpectChain = [this, Interval](AActor& Target, int32 Chain, const TCHAR* Why) {
				Wait(Interval);
				const EVeyraAttackRejection Rejection = AttackNow(Target);
				ASSERT_THAT(IsTrue(Rejection == EVeyraAttackRejection::None && Attacks->GetChain() == Chain,
					FString::Printf(TEXT("%s: %s, chain %d at %g s, next attack at %g s"), Why, LexToString(Rejection), Attacks->GetChain(),
						Spawner.GetWorld().GetTimeSeconds(), Attacks->GetNextAttackAt())));
			};

			ExpectChain(First, 1, TEXT("the first attack"));
			ExpectChain(First, 2, TEXT("the same target again"));
			ExpectChain(Second, 1, TEXT("a new target starts again"));
			ExpectChain(Fluxborn, 0, TEXT("only Vanguards build the chain"));
			ExpectChain(First, 1, TEXT("back to a Vanguard"));
			Attacker->GetPlayerState()->FindComponentByClass<UVeyraCombatStateComponent>()->Clear();
			ASSERT_THAT(IsTrue(Attacks->GetChain() == 0 && Attacks->GetChainTarget() == nullptr, TEXT("leaving combat drops it")));
		}

		TEST_METHOD(ARangedAttackLandsWhenItsProjectileArrives)
		{
			// Fixture values: a ranged profile, and its projectile's flight.
			constexpr double RangedRange = 600.0;
			constexpr double ShotSpeed = 1000.0;
			constexpr double ShotRadius = 10.0;
			FVeyraBasicAttackProfile Ranged = Melee();
			Ranged.Range = RangedRange;
			Ranged.Projectile.Add(FVeyraAttackProjectileTuning{ ShotSpeed, ShotRadius });
			ASSERT_THAT(IsTrue(Attacks->SetProfile(Ranged)));
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(RangedRange / 2.0, 0.0, 0.0));
			int32 Hit = 0;
			Attacks->OnHit.AddLambda([&Hit](const FVeyraAttackEvent&) { ++Hit; });

			ASSERT_THAT(IsTrue(AttackNow(Enemy) == EVeyraAttackRejection::None));
			ASSERT_THAT(IsTrue(World.HealthLost(Enemy) == 0.0 && Hit == 0, TEXT("a ranged attack lands when its projectile does")));
			TActorIterator<AVeyraProjectile> Shot(&Spawner.GetWorld());
			ASSERT_THAT(IsTrue(Shot && Shot->GetHomingTarget() == &Enemy));
			// Once launched it cannot be escaped by leaving range (Combat Bible §4).
			Enemy.SetActorLocation(FVector(RangedRange * 2.0, 0.0, 0.0));
			Shot->AdvanceBy(RangedRange * 3.0 / ShotSpeed);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Enemy), BaseDamage(), Tolerance) && Hit == 1));
		}

		TEST_METHOD(ValidationCatchesWhatTheSchemaCannot)
		{
			constexpr int32 RankCounts[] = { 5, 3 };
			ASSERT_THAT(IsTrue(VeyraAbilityRules::Validate(Tuning, RankCounts).IsEmpty(), FString::Join(VeyraAbilityRules::Validate(Tuning, RankCounts), TEXT(" | "))));
			FVeyraAbilitiesTuning Broken = Tuning;
			FVeyraEmpoweredAttackAbilityTuning& Heavy = Broken.EmpoweredAttack.FindChecked(ArchetypeTestId(TEXT("test_heavy")));
			Heavy.ArmorPenetrationByRank = { 1.5 };
			Heavy.SecondaryImpact[0].Shape.Radius = 0.0;
			const TArray<FString> Problems = VeyraAbilityRules::Validate(Broken, RankCounts);
			const FString All = FString::Join(Problems, TEXT(" | "));
			const auto Mentions = [&Problems](const TCHAR* Pointer) { return Problems.ContainsByPredicate([Pointer](const FString& Problem) { return Problem.StartsWith(Pointer); }); };
			ASSERT_THAT(IsTrue(Mentions(TEXT("/empoweredAttack/test_heavy/armorPenetrationByRank:")), All));
			ASSERT_THAT(IsTrue(Mentions(TEXT("/empoweredAttack/test_heavy/secondaryImpact/0/shape:")), All));

			// A cleave takes part of the attack's damage, never more than all of it.
			FVeyraStatusSpec Cleave = UVeyraAbilitiesTuningSubsystem::FindStatus(ArchetypeTestId(TEXT("test_cleave"))).GetValue();
			ASSERT_THAT(IsTrue(VeyraStatuses::Validate(Cleave).IsEmpty()));
			Cleave.Magnitude = 1.5;
			ASSERT_THAT(IsFalse(VeyraStatuses::Validate(Cleave).IsEmpty()));
		}

		TEST_METHOD(AProfileOutsideTheRulesIsRefused)
		{
			TestRunner->AddExpectedMessagePlain(TEXT("Refused a basic attack profile"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1);
			FVeyraBasicAttackProfile Broken = Melee();
			Broken.WindupFraction = 1.0;
			Broken.Range = 0.0;
			const TArray<FString> Problems = VeyraBasicAttacks::Validate(Broken);
			ASSERT_THAT(AreEqual(2, Problems.Num()));
			ASSERT_THAT(IsFalse(Attacks->SetProfile(Broken)));
			ASSERT_THAT(IsTrue(Attacks->GetProfile().Range == Range, TEXT("the valid profile stays")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
