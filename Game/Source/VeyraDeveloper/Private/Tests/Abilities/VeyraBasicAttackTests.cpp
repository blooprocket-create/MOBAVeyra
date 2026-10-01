// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attacks/VeyraBasicAttackComponent.h"
#include "Attributes/VeyraOffenceSet.h"
#include "CombatState/VeyraCombatStateComponent.h"
#include "CQTest.h"
#include "VeyraVisionSubsystem.h"
#include "Delivery/VeyraProjectile.h"
#include "EngineUtils.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Life/VeyraLifeComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"
#include "VeyraCombatVerbs.h"

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
		static constexpr double RangeMargin = 10.0;
		static constexpr int32 TripleAttacks = 3;
		static constexpr double QuickWindup = 0.5;
		static constexpr double PierceSeconds = 3.0;
		static constexpr double BriefSeconds = 0.2;

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

			// Three quicker empowered attacks, as Doubletime's (ADR-027 §2).
			FVeyraEmpoweredAttackAbilityTuning Triple;
			Triple.Cast = InstantCast(0.0, LongSeconds, 0.0);
			Triple.DurationSeconds = LongSeconds;
			Triple.Attacks = TripleAttacks;
			Triple.WindupScale = QuickWindup;
			Triple.Damage.Add(FVeyraDamageTuning{ EVeyraDamageType::TrueDamage, { BonusDamage }, 0.0, 0.0 });
			Triple.ArmorPenetrationByRank = { 0.0 };
			Tuning.EmpoweredAttack.Add(ArchetypeTestId(TEXT("test_triple")), Triple);

			// A quick empowerment that lapses soon.
			FVeyraEmpoweredAttackAbilityTuning Brief = Triple;
			Brief.DurationSeconds = BriefSeconds;
			Brief.Attacks = 1;
			Tuning.EmpoweredAttack.Add(ArchetypeTestId(TEXT("test_brief")), Brief);

			// A buff whose attacks pierce behind their target for a while, as OPEN ROAD!'s (ADR-027 §3).
			FVeyraSelfBuffAbilityTuning Road;
			Road.Cast = InstantCast(0.0, LongSeconds, 0.0);
			FVeyraBuffAttackImpactTuning& Pierce = Road.AttackSecondaryImpact.AddDefaulted_GetRef();
			Pierce.Seconds = PierceSeconds;
			Pierce.Impact.Priority = 1;
			Pierce.Impact.Shape = CircleOf(ImpactRadius);
			Pierce.Impact.Damage.Add(FVeyraDamageTuning{ EVeyraDamageType::TrueDamage, { ImpactDamage }, 0.0, 0.0 });
			Tuning.SelfBuff.Add(ArchetypeTestId(TEXT("test_road")), Road);
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);

			FArchetypeTestWorld World{ Spawner };
			Attacker = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			Attacks = Attacker->GetPlayerState()->FindComponentByClass<UVeyraBasicAttackComponent>();
			ASSERT_THAT(IsTrue(Attacks && Attacks->SetProfile(Melee())));
		}

		TEST_METHOD(AMobileAttackerKeepsItsShareOfSpeedThroughItsWindupOnly)
		{
			// Fixture values: a passive's half, and a status's whole.
			constexpr double Half = 0.5;
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Range / 2.0, 0.0, 0.0));
			const UVeyraMovementComponent& Movement = *Attacker->GetVeyraMovement();
			const double Walking = Movement.GetMaxSpeed();
			ASSERT_THAT(IsTrue(Attacks->StartAttack(Enemy) == EVeyraAttackRejection::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Movement.GetMaxSpeed(), Walking, Tolerance), TEXT("a standing attacker is stopped by its orders, not its speed")));
			Attacks->CancelAttack();
			Attacks->SetWindupMovement(Half);
			ASSERT_THAT(IsTrue(Attacks->StartAttack(Enemy) == EVeyraAttackRejection::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Movement.GetMaxSpeed(), Walking * Half, Tolerance), TEXT("half its speed through the windup (ADR-027 §1)")));
			Attacks->Commit();
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Movement.GetMaxSpeed(), Walking, Tolerance), TEXT("its whole speed after Commit")));
			FVeyraStatusSpec Road;
			Road.Id = ArchetypeTestId(TEXT("test_open_road"));
			Road.Kind = EVeyraStatusKind::MobileAttack;
			Road.Magnitude = 1.0;
			Road.DurationSeconds = LongSeconds;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Attacker->GetAbilitySystemComponent(), *Attacker->GetAbilitySystemComponent(), Road)));
			ASSERT_THAT(IsTrue(Attacks->GetWindupMovementShare() == 1.0, TEXT("the strongest applies")));
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

		/** A status record for ApplyStatus. Fixture values. */
		static FVeyraStatusSpec Mark(const TCHAR* Id, EVeyraStatusKind Kind, double Magnitude)
		{
			FVeyraStatusSpec Spec;
			Spec.Id = ArchetypeTestId(Id);
			Spec.Kind = Kind;
			Spec.Magnitude = Magnitude;
			Spec.DurationSeconds = LongSeconds;
			return Spec;
		}

		TEST_METHOD(RangeGrowsWithAttackRangeAndWithTheTargetsMarksFromThisAttacker)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Range * 3.0, 0.0, 0.0));
			AVeyraVanguardCharacter& Ally = World.Spawn(EVeyraTeam::A, FVector(0.0, Range * 3.0, 0.0));
			const double Gap = VeyraTargeting::EdgeToEdgeDistance(*Attacker, Enemy);
			ASSERT_THAT(IsTrue(Gap > Range && Attacks->CheckAttack(&Enemy) == EVeyraAttackRejection::OutOfRange));
			const double Extra = Gap - Range + RangeMargin;
			UAbilitySystemComponent& Self = *Attacker->GetAbilitySystemComponent();
			UAbilitySystemComponent& Target = *Enemy.GetAbilitySystemComponent();

			// A mark reaches further only for the unit that placed it, as Kade's Tracked does (ADR-018 §2).
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Ally.GetAbilitySystemComponent(), Target, Mark(TEXT("test_ally_mark"), EVeyraStatusKind::SourceAttackRange, Extra))));
			ASSERT_THAT(IsTrue(Attacks->CheckAttack(&Enemy) == EVeyraAttackRejection::OutOfRange && Attacks->GetRange(&Enemy) == Range));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Self, Target, Mark(TEXT("test_mark"), EVeyraStatusKind::SourceAttackRange, Extra))));
			ASSERT_THAT(IsTrue(Attacks->CheckAttack(&Enemy) == EVeyraAttackRejection::None && Attacks->GetRange(nullptr) == Range));
			ASSERT_THAT(IsTrue(VeyraCombat::RemoveStatus(Target, ArchetypeTestId(TEXT("test_mark")))));
			ASSERT_THAT(IsTrue(Attacks->CheckAttack(&Enemy) == EVeyraAttackRejection::OutOfRange));

			// Its own AttackRange reaches every target further.
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Self, Self, Mark(TEXT("test_reach"), EVeyraStatusKind::AttackRange, Extra))));
			ASSERT_THAT(IsTrue(Attacks->CheckAttack(&Enemy) == EVeyraAttackRejection::None && FMath::IsNearlyEqual(Attacks->GetRange(nullptr), Range + Extra)));
		}

		TEST_METHOD(AnAttackOnATargetThatVanishesBeforeCommitIsCancelled)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Range / 2.0, 0.0, 0.0));
			UVeyraVisionSubsystem& Vision = *Spawner.GetWorld().GetSubsystem<UVeyraVisionSubsystem>();
			Vision.Start();
			ASSERT_THAT(IsTrue(Attacks->StartAttack(Enemy) == EVeyraAttackRejection::None));
			// Camouflaged during the windup, beyond its detection radius: at Commit it is NotVisible (ADR-018 §4).
			ASSERT_THAT(IsTrue(VeyraCombatTests::Camouflage(Enemy, Range / 4.0)));
			Vision.UpdateNow();
			Attacks->Commit();
			ASSERT_THAT(IsTrue(World.HealthLost(Enemy) == 0.0 && Attacks->GetState().Phase == EVeyraAttackPhase::None));
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
			// Presentation sees which ability waits, and until when.
			const FVeyraAttackEmpowermentView& View = Attacks->GetEmpowermentView();
			ASSERT_THAT(IsTrue(View.Ability == ArchetypeTestId(TEXT("test_heavy"))));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(View.ExpiresAt - Spawner.GetWorld().GetTimeSeconds(), LongSeconds, Tolerance)));
			bool bEmpoweredEvent = false;
			Attacks->OnAttack.AddLambda([&bEmpoweredEvent](const FVeyraAttackEvent& Event) { bEmpoweredEvent = Event.bEmpowered; });

			ASSERT_THAT(IsTrue(AttackNow(Enemy) == EVeyraAttackRejection::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Enemy), BaseDamage() + BonusDamage, Tolerance), TEXT("one hit, with the bonus in it")));
			ASSERT_THAT(IsTrue(World.Has(Enemy, TEXT("test_slow")) && bEmpoweredEvent));
			ASSERT_THAT(IsFalse(Attacks->IsEmpowered(), TEXT("the attack consumed it")));
			ASSERT_THAT(IsFalse(Attacks->GetEmpowermentView().Ability.IsValid(), TEXT("and presentation no longer shows it")));
		}

		TEST_METHOD(AnEmpowermentWithChargesEmpowersThatManyQuickerAttacks)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(100.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(World.Learn(*Attacker, EVeyraAbilitySlot::Q, ArchetypeTestId(TEXT("test_triple")))));
			ASSERT_THAT(IsTrue(VeyraAbilities::TryCast(*Attacker->GetAbilitySystemComponent(), EVeyraAbilitySlot::Q, FVeyraCastTarget()) == EVeyraCastRejection::None));
			ASSERT_THAT(AreEqual(TripleAttacks, Attacks->GetEmpowermentView().Attacks, TEXT("presentation sees how many are left")));
			const double Interval = Attacks->GetTiming().IntervalSeconds;
			// The empowered attacks, then one plain attack after them.
			for (int32 Attack = 1; Attack <= TripleAttacks + 1; ++Attack)
			{
				Wait(Interval);
				const double Now = Spawner.GetWorld().GetTimeSeconds();
				ASSERT_THAT(IsTrue(Attacks->StartAttack(Enemy) == EVeyraAttackRejection::None));
				const double Windup = Attacks->GetState().PhaseEndsAt - Now;
				const bool bEmpowered = Attack <= TripleAttacks;
				const double ExpectedWindup = Interval * WindupFraction * (bEmpowered ? QuickWindup : 1.0);
				ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Windup, ExpectedWindup, Tolerance), FString::Printf(TEXT("attack %d winds up for %g s"), Attack, Windup)));
				Attacks->Commit();
				const double ExpectedLost = Attack * BaseDamage() + FMath::Min(Attack, TripleAttacks) * BonusDamage;
				ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Enemy), ExpectedLost, Tolerance), FString::Printf(TEXT("after attack %d, lost %g"), Attack, World.HealthLost(Enemy))));
				ASSERT_THAT(AreEqual(FMath::Max(TripleAttacks - Attack, 0), Attacks->GetEmpowermentView().Attacks, FString::Printf(TEXT("left after attack %d"), Attack)));
			}
			ASSERT_THAT(IsFalse(Attacks->IsEmpowered(), TEXT("the last empowered attack spent it")));
		}

		TEST_METHOD(AnEmpowermentThatLapsesMidWindupStillEmpowersTheAttackItQuickened)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(100.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(World.Learn(*Attacker, EVeyraAbilitySlot::Q, ArchetypeTestId(TEXT("test_brief")))));
			ASSERT_THAT(IsTrue(VeyraAbilities::TryCast(*Attacker->GetAbilitySystemComponent(), EVeyraAbilitySlot::Q, FVeyraCastTarget()) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Attacks->StartAttack(Enemy) == EVeyraAttackRejection::None));
			Wait(BriefSeconds * 2.0);
			ASSERT_THAT(IsFalse(Attacks->IsEmpowered(), TEXT("its time ran out during the windup")));
			Attacks->Commit();
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Enemy), BaseDamage() + BonusDamage, Tolerance),
				FString::Printf(TEXT("the quickened attack keeps its bonus: lost %g"), World.HealthLost(Enemy))));
		}

		TEST_METHOD(ABuffsSecondaryImpactRidesItsCastersAttacksWhileItLasts)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Primary = World.Spawn(EVeyraTeam::B, FVector(100.0, 0.0, 0.0));
			AVeyraVanguardCharacter& BehindIt = World.Spawn(EVeyraTeam::B, FVector(100.0 + ImpactRadius / 2.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(World.Learn(*Attacker, EVeyraAbilitySlot::E, ArchetypeTestId(TEXT("test_road")))));
			ASSERT_THAT(IsTrue(VeyraAbilities::TryCast(*Attacker->GetAbilitySystemComponent(), EVeyraAbilitySlot::E, FVeyraCastTarget()) == EVeyraCastRejection::None));
			const double Interval = Attacks->GetTiming().IntervalSeconds;

			ASSERT_THAT(IsTrue(AttackNow(Primary) == EVeyraAttackRejection::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(BehindIt), ImpactDamage, Tolerance), TEXT("the buff's impact lands behind the target")));
			Wait(Interval);
			ASSERT_THAT(IsTrue(AttackNow(Primary) == EVeyraAttackRejection::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(BehindIt), 2.0 * ImpactDamage, Tolerance), TEXT("on every attack while it lasts")));
			Wait(PierceSeconds);
			ASSERT_THAT(IsTrue(AttackNow(Primary) == EVeyraAttackRejection::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(BehindIt), 2.0 * ImpactDamage, Tolerance), TEXT("and on none after")));
		}

		TEST_METHOD(AStructureTakesTheAttackInFullAndItsRidersAtStructureEffectiveness)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraTestStructure& Spire = World.SpawnStructure(EVeyraTeam::B, FVector(100.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(World.Learn(*Attacker, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_heavy")))));
			ASSERT_THAT(IsTrue(VeyraAbilities::TryCast(*Attacker->GetAbilitySystemComponent(), EVeyraAbilitySlot::W, FVeyraCastTarget()) == EVeyraCastRejection::None));

			ASSERT_THAT(IsTrue(AttackNow(Spire) == EVeyraAttackRejection::None, TEXT("basic attacks may target structures")));
			const double Effectiveness = UVeyraCombatTuningSubsystem::Get().Structures.Effectiveness;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Spire), BaseDamage() + BonusDamage * Effectiveness, Tolerance),
				FString::Printf(TEXT("lost %g"), World.HealthLost(Spire))));
			ASSERT_THAT(IsFalse(World.Has(Spire, TEXT("test_slow")), TEXT("the empowerment's slow does not affect a structure")));
		}

		TEST_METHOD(ACertainCritMultipliesTheBaseDamageButNotTheRiders)
		{
			// Full Crit Chance crits on every roll in [0, 1) (Combat Bible §5).
			Attacker->GetAbilitySystemComponent()->SetNumericAttributeBase(UVeyraOffenceSet::GetCritChanceAttribute(), 1.0f);
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(100.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(World.Learn(*Attacker, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_heavy")))));
			ASSERT_THAT(IsTrue(VeyraAbilities::TryCast(*Attacker->GetAbilitySystemComponent(), EVeyraAbilitySlot::W, FVeyraCastTarget()) == EVeyraCastRejection::None));
			bool bCritical = false;
			Attacks->OnHit.AddLambda([&bCritical](const FVeyraAttackEvent& Event) { bCritical = Event.bCritical; });
			bool bDealtCritical = false;
			Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>()->OnDamageDealt.AddLambda([&bDealtCritical](const FVeyraDamageDealtEvent& Event) {
				bDealtCritical |= Event.Delivery == EVeyraDamageDelivery::BasicAttack && Event.bCritical;
			});

			ASSERT_THAT(IsTrue(AttackNow(Enemy) == EVeyraAttackRejection::None));
			// The empowerment's bonus damage is a rider (§17): it does not crit.
			const double CritDamage = UVeyraCombatTuningSubsystem::Get().Crit.Damage;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Enemy), BaseDamage() * CritDamage + BonusDamage, Tolerance),
				FString::Printf(TEXT("lost %g"), World.HealthLost(Enemy))));
			ASSERT_THAT(IsTrue(bCritical, TEXT("the hit says it crit")));
			ASSERT_THAT(IsTrue(bDealtCritical, TEXT("and so does the damage it dealt (ADR-025 §6)")));
		}

		TEST_METHOD(AStructureTakesACritsBonusAtStructureEffectiveness)
		{
			Attacker->GetAbilitySystemComponent()->SetNumericAttributeBase(UVeyraOffenceSet::GetCritChanceAttribute(), 1.0f);
			FArchetypeTestWorld World{ Spawner };
			AVeyraTestStructure& Spire = World.SpawnStructure(EVeyraTeam::B, FVector(100.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(AttackNow(Spire) == EVeyraAttackRejection::None));
			// The crit bonus is a rider, which a structure takes at Structure Effectiveness (§33).
			const double Bonus = BaseDamage() * (UVeyraCombatTuningSubsystem::Get().Crit.Damage - 1.0);
			const double Effectiveness = UVeyraCombatTuningSubsystem::Get().Structures.Effectiveness;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Spire), BaseDamage() + Bonus * Effectiveness, Tolerance),
				FString::Printf(TEXT("lost %g"), World.HealthLost(Spire))));
		}

		TEST_METHOD(WithoutCritChanceNothingCrits)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(100.0, 0.0, 0.0));
			bool bCritical = true;
			Attacks->OnAttack.AddLambda([&bCritical](const FVeyraAttackEvent& Event) { bCritical = Event.bCritical; });
			ASSERT_THAT(IsTrue(AttackNow(Enemy) == EVeyraAttackRejection::None));
			ASSERT_THAT(IsFalse(bCritical));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(Enemy), BaseDamage(), Tolerance)));
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
			Heavy.Attacks = 0;
			Heavy.WindupScale = 1.5;
			FVeyraBuffAttackImpactTuning& Pierce = Broken.SelfBuff.FindChecked(ArchetypeTestId(TEXT("test_road"))).AttackSecondaryImpact[0];
			Pierce.Seconds = 0.0;
			Pierce.Impact.Shape.Radius = 0.0;
			const TArray<FString> Problems = VeyraAbilityRules::Validate(Broken, RankCounts);
			const FString All = FString::Join(Problems, TEXT(" | "));
			const auto Mentions = [&Problems](const TCHAR* Pointer) { return Problems.ContainsByPredicate([Pointer](const FString& Problem) { return Problem.StartsWith(Pointer); }); };
			ASSERT_THAT(IsTrue(Mentions(TEXT("/empoweredAttack/test_heavy/armorPenetrationByRank:")), All));
			ASSERT_THAT(IsTrue(Mentions(TEXT("/empoweredAttack/test_heavy/secondaryImpact/0/shape:")), All));
			ASSERT_THAT(IsTrue(Mentions(TEXT("/empoweredAttack/test_heavy/attacks:")), All));
			ASSERT_THAT(IsTrue(Mentions(TEXT("/empoweredAttack/test_heavy/windupScale:")), All));
			ASSERT_THAT(IsTrue(Mentions(TEXT("/selfBuff/test_road/attackSecondaryImpact/0/seconds:")), All));
			ASSERT_THAT(IsTrue(Mentions(TEXT("/selfBuff/test_road/attackSecondaryImpact/0/impact/shape:")), All));

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
