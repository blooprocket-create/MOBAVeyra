// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attributes/VeyraDefenceSet.h"
#include "Attributes/VeyraMobilitySet.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "CQTest.h"
#include "Engine/World.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Life/VeyraLifeComponent.h"
#include "Statuses/VeyraStatusComponent.h"
#include "TimerManager.h"
#include "Tests/Combat/VeyraCombatTestHelpers.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCombatTests
{
	/** A status record as an ability's data would declare it. Test fixture values. */
	inline FVeyraStatusSpec TestStatus(const TCHAR* Id, EVeyraStatusKind Kind, double Magnitude, double DurationSeconds,
		EVeyraStackingPolicy Stacking = EVeyraStackingPolicy::UniqueRefresh, int32 MaxStacks = 1)
	{
		FVeyraStatusSpec Spec;
		Spec.Id = FVeyraContentId::FromText(Id).GetValue();
		Spec.Kind = Kind;
		Spec.Stacking = Stacking;
		Spec.Magnitude = Magnitude;
		Spec.DurationSeconds = DurationSeconds;
		Spec.MaxStacks = MaxStacks;
		return Spec;
	}

	// Veyra.Combat.Statuses.*: the status ledger's stacking policies, Tenacity, crowd control and its
	// action blocks, and the stat changes statuses make (Combat Bible §8, §41, §44, §46; ADR-009 §1).
	TEST_CLASS(Statuses, "Veyra.Combat")
	{
		// Fixture values: a Tenacity floor to check against, and a long duration for statuses that
		// must not end during a test.
		static constexpr double TestTenacityFloorSeconds = 0.5;
		static constexpr double LongSeconds = 60.0;

		FVeyraCombatTuning Tuning;
		FActorTestSpawner Spawner;
		UAbilitySystemComponent* Caster = nullptr;
		UAbilitySystemComponent* Unit = nullptr;
		UVeyraStatusComponent* StatusLedger = nullptr;

		BEFORE_EACH()
		{
			Tuning = UVeyraCombatTuningSubsystem::Get();
			Tuning.CrowdControl.TenacityFloorSeconds = TestTenacityFloorSeconds;
			UVeyraCombatTuningSubsystem::SetTestOverride(&Tuning);
			Caster = &SpawnCombatant(Spawner);
			Unit = &SpawnCombatant(Spawner);
			ASSERT_THAT(IsTrue(VeyraCombat::InitializeStats(*Caster, ExampleStats())));
			ASSERT_THAT(IsTrue(VeyraCombat::InitializeStats(*Unit, ExampleStats())));
			StatusLedger = Unit->GetOwner()->FindComponentByClass<UVeyraStatusComponent>();
			ASSERT_THAT(IsNotNull(StatusLedger));
		}

		AFTER_EACH()
		{
			UVeyraCombatTuningSubsystem::SetTestOverride(nullptr);
		}

		const FVeyraStatusEntry* Find(const TCHAR* Id) const
		{
			const FVeyraContentId StatusId = FVeyraContentId::FromText(Id).GetValue();
			return StatusLedger->GetLedger().Entries.FindByPredicate([&StatusId](const FVeyraStatusEntry& Entry) { return Entry.Id == StatusId; });
		}

		double Value(const FGameplayAttribute& Attribute) const
		{
			return Unit->GetNumericAttribute(Attribute);
		}

		double Duration(const TCHAR* Id) const
		{
			const FVeyraStatusEntry* Entry = Find(Id);
			return Entry ? Entry->EndsAt - Entry->StartedAt : 0.0;
		}

		TEST_METHOD(ARefreshReplacesTheStatusAndItsStatChange)
		{
			constexpr double First = 0.1;
			constexpr double Second = 0.2;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, TestStatus(TEXT("haste"), EVeyraStatusKind::MoveSpeed, First, LongSeconds))));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, TestStatus(TEXT("haste"), EVeyraStatusKind::MoveSpeed, Second, LongSeconds))));
			ASSERT_THAT(AreEqual(1, StatusLedger->GetLedger().Entries.Num()));
			ASSERT_THAT(IsTrue(Find(TEXT("haste"))->Magnitude == Second));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Value(UVeyraMobilitySet::GetMoveSpeedAttribute()), ExampleStats().MoveSpeed * (1.0 + Second), 1e-3)));
		}

		TEST_METHOD(ReplaceStrongestKeepsTheStrongerApplication)
		{
			constexpr double Strong = 0.4;
			constexpr double Weak = 0.2;
			constexpr double Stronger = 0.6;
			constexpr EVeyraStackingPolicy Policy = EVeyraStackingPolicy::UniqueReplaceStrongest;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, TestStatus(TEXT("chill"), EVeyraStatusKind::Slow, Strong, LongSeconds, Policy))));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, TestStatus(TEXT("chill"), EVeyraStatusKind::Slow, Weak, LongSeconds * 2.0, Policy))));
			ASSERT_THAT(IsTrue(Find(TEXT("chill"))->Magnitude == Strong, TEXT("a weaker application replaced a stronger one")));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, TestStatus(TEXT("chill"), EVeyraStatusKind::Slow, Stronger, LongSeconds, Policy))));
			ASSERT_THAT(IsTrue(Find(TEXT("chill"))->Magnitude == Stronger));
			ASSERT_THAT(AreEqual(1, StatusLedger->GetLedger().Entries.Num()));
		}

		TEST_METHOD(StacksAddUpToTheCap)
		{
			constexpr double PerStack = 0.1;
			constexpr int32 MaxStacks = 3;
			const FVeyraStatusSpec Spec = TestStatus(TEXT("fervour"), EVeyraStatusKind::AttackSpeed, PerStack, LongSeconds, EVeyraStackingPolicy::Stacking, MaxStacks);
			for (int32 Application = 0; Application <= MaxStacks; ++Application)
			{
				ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, Spec)));
			}
			ASSERT_THAT(AreEqual(MaxStacks, Find(TEXT("fervour"))->Stacks));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Value(UVeyraOffenceSet::GetAttackSpeedAttribute()), ExampleStats().AttackSpeed * (1.0 + PerStack * MaxStacks), 1e-5)));
		}

		TEST_METHOD(TraitsChangeHealthRegenerationAndDamageDealt)
		{
			// The jungle's sustain and impact traits (ADR-014 §2). Fixture values.
			constexpr double Regeneration = 5.0;
			constexpr double Doubled = 1.0;
			constexpr double Amplified = 0.1;
			FVeyraStatBlock Stats = ExampleStats();
			Stats.HealthRegen = Regeneration;
			UAbilitySystemComponent& Regenerating = SpawnCombatant(Spawner);
			ASSERT_THAT(IsTrue(VeyraCombat::InitializeStats(Regenerating, Stats)));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, Regenerating, TestStatus(TEXT("mire"), EVeyraStatusKind::HealthRegeneration, Doubled, LongSeconds))));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Regenerating.GetNumericAttribute(UVeyraVitalsSet::GetHealthRegenAttribute()), Regeneration * (1.0 + Doubled), 1e-4)));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, Regenerating, TestStatus(TEXT("tusk"), EVeyraStatusKind::DamageAmplification, Amplified, LongSeconds))));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Regenerating.GetNumericAttribute(UVeyraOffenceSet::GetOutgoingDamageMultiplierAttribute()), 1.0 + Amplified, 1e-5)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Regenerating.GetNumericAttribute(UVeyraVitalsSet::GetHealthRegenAttribute()), Regeneration * (1.0 + Doubled), 1e-4),
				TEXT("each status changes its own stat only")));
			// Amplification adds; none, or more than doubling in all, is refused.
			ASSERT_THAT(IsFalse(VeyraStatuses::Validate(TestStatus(TEXT("tusk"), EVeyraStatusKind::DamageAmplification, 0.0, LongSeconds)).IsEmpty()));
			ASSERT_THAT(IsFalse(VeyraStatuses::Validate(TestStatus(TEXT("tusk"), EVeyraStatusKind::DamageAmplification, 0.6, LongSeconds, EVeyraStackingPolicy::Stacking, 2)).IsEmpty()));
		}

		/** Two frames on the world's timers, the second Seconds long: world time stands still in a test, and the first frame only activates timers. */
		void AdvanceTimers(double Seconds)
		{
			FTimerManager& Timers = Spawner.GetWorld().GetTimerManager();
			++GFrameCounter;
			Timers.Tick(0.0f);
			++GFrameCounter;
			Timers.Tick(static_cast<float>(Seconds));
		}

		double HealthLost() const
		{
			return Value(UVeyraVitalsSet::GetMaxHealthAttribute()) - Value(UVeyraVitalsSet::GetHealthAttribute());
		}

		static FVeyraStatusSpec Burn(double PerTick, double TickSeconds, double DurationSeconds)
		{
			FVeyraStatusSpec Spec = TestStatus(TEXT("burn"), EVeyraStatusKind::DamageOverTime, PerTick, DurationSeconds);
			Spec.DamageType = EVeyraDamageType::TrueDamage;
			Spec.TickSeconds = TickSeconds;
			return Spec;
		}

		TEST_METHOD(DamageOverTimeTicksWholeTicksAndNoneAsItLands)
		{
			// Fixture values: 10 True damage a second for 3 s, and a moment beyond.
			constexpr double PerTick = 10.0;
			constexpr double TickSeconds = 1.0;
			constexpr double Lasts = 3.0;
			constexpr double Margin = 0.1;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, Burn(PerTick, TickSeconds, Lasts))));
			ASSERT_THAT(IsTrue(HealthLost() == 0.0, TEXT("no tick as it lands (Combat Bible §14)")));
			AdvanceTimers(Lasts + Margin);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HealthLost(), PerTick * Lasts / TickSeconds, 1e-3),
				FString::Printf(TEXT("three ticks, the last as it ends: lost %.1f"), HealthLost())));
			// Its effect ends on the world's clock, which a test holds still; its ticks are counted.
			AdvanceTimers(Lasts);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HealthLost(), PerTick * Lasts / TickSeconds, 1e-3), TEXT("and no more")));
		}

		TEST_METHOD(ARefreshRestartsTheTicksAndRemovalStopsThem)
		{
			constexpr double PerTick = 10.0;
			constexpr double TickSeconds = 1.0;
			constexpr double Lasts = 3.0;
			constexpr double Margin = 0.1;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, Burn(PerTick, TickSeconds, Lasts))));
			AdvanceTimers(TickSeconds + Margin);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HealthLost(), PerTick, 1e-3)));
			// Reapplied from the same source: its duration and its ticks start again (§14).
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, Burn(PerTick, TickSeconds, Lasts))));
			AdvanceTimers(TickSeconds / 2.0);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HealthLost(), PerTick, 1e-3), TEXT("no tick half a second into the new application")));
			ASSERT_THAT(IsTrue(VeyraCombat::RemoveStatus(*Unit, FVeyraContentId::FromText(TEXT("burn")).GetValue())));
			AdvanceTimers(Lasts + Margin);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HealthLost(), PerTick, 1e-3), TEXT("removed early, it ticks no more")));
		}

		TEST_METHOD(ALethalTickKillsInItsSourcesName)
		{
			constexpr double TickSeconds = 1.0;
			constexpr double Margin = 0.1;
			TWeakObjectPtr<UAbilitySystemComponent> Killer;
			Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>()->OnDeath.AddLambda([&Killer](const FVeyraDeathEvent& Death) { Killer = Death.Killer; });
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, Burn(ExampleStats().MaxHealth, TickSeconds, TickSeconds * 2.0))));
			AdvanceTimers(TickSeconds + Margin);
			ASSERT_THAT(IsTrue(Killer.Get() == Caster, TEXT("the burn's source killed it")));
			ASSERT_THAT(IsNull(Find(TEXT("burn")), TEXT("death ends it")));
		}

		TEST_METHOD(WeakenCutsTheDamageTheUnitDeals)
		{
			constexpr double Cut = 0.35;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, TestStatus(TEXT("frail"), EVeyraStatusKind::Weaken, Cut, LongSeconds))));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Value(UVeyraOffenceSet::GetOutgoingDamageMultiplierAttribute()), 1.0 - Cut, 1e-5)));
			// A cut of all of it, or a tick on any kind but a damage over time, is refused.
			ASSERT_THAT(IsFalse(VeyraStatuses::Validate(TestStatus(TEXT("frail"), EVeyraStatusKind::Weaken, 1.0, LongSeconds)).IsEmpty()));
			FVeyraStatusSpec TickingSlow = TestStatus(TEXT("mire"), EVeyraStatusKind::Slow, 0.2, LongSeconds);
			TickingSlow.TickSeconds = 1.0;
			ASSERT_THAT(IsFalse(VeyraStatuses::Validate(TickingSlow).IsEmpty()));
			ASSERT_THAT(IsFalse(VeyraStatuses::Validate(Burn(10.0, 0.0, LongSeconds)).IsEmpty(), TEXT("a damage over time needs its tick")));
			ASSERT_THAT(IsFalse(VeyraStatuses::Validate(Burn(10.0, LongSeconds * 2.0, LongSeconds)).IsEmpty(), TEXT("and at least one in its duration")));
		}

		TEST_METHOD(IndependentSourcesKeepOneInstanceEach)
		{
			constexpr double Reduction = 0.2;
			UAbilitySystemComponent& OtherCaster = SpawnCombatant(Spawner);
			const FVeyraStatusSpec Spec = TestStatus(TEXT("bulwark"), EVeyraStatusKind::DamageReduction, Reduction, LongSeconds, EVeyraStackingPolicy::IndependentSources);
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, Spec)));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(OtherCaster, *Unit, Spec)));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, Spec)));
			ASSERT_THAT(AreEqual(2, StatusLedger->GetLedger().Entries.Num()));
			// Percentage reductions stack multiplicatively (Combat Bible §41).
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Value(UVeyraDefenceSet::GetIncomingDamageMultiplierAttribute()), (1.0 - Reduction) * (1.0 - Reduction), 1e-5)));
		}

		TEST_METHOD(TheStrongestSlowControlsAndAWeakerOneTakesOver)
		{
			constexpr double Weak = 0.2;
			constexpr double Strong = 0.5;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, TestStatus(TEXT("mire"), EVeyraStatusKind::Slow, Weak, LongSeconds))));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, TestStatus(TEXT("frost"), EVeyraStatusKind::Slow, Strong, LongSeconds))));
			ASSERT_THAT(IsTrue(StatusLedger->GetStrongestSlow() == Strong));
			ASSERT_THAT(IsTrue(VeyraCombat::RemoveStatus(*Unit, FVeyraContentId::FromText(TEXT("frost")).GetValue())));
			ASSERT_THAT(IsTrue(StatusLedger->GetStrongestSlow() == Weak, TEXT("the weaker Slow should still be tracked")));
		}

		TEST_METHOD(TenacityShortensCrowdControlDownToTheFloor)
		{
			constexpr double Tenacity = 0.4;
			constexpr double Retained = 1.0 - Tenacity;
			constexpr double LongStun = 2.0;
			constexpr double NearTheFloor = 0.6;
			constexpr double BelowTheFloor = 0.4;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Unit, *Unit, TestStatus(TEXT("grit"), EVeyraStatusKind::Tenacity, Tenacity, LongSeconds))));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Value(UVeyraDefenceSet::GetTenacityRetainedAttribute()), Retained, 1e-5)));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, TestStatus(TEXT("long_stun"), EVeyraStatusKind::Stun, 0.0, LongStun))));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, TestStatus(TEXT("floor_stun"), EVeyraStatusKind::Stun, 0.0, NearTheFloor))));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, TestStatus(TEXT("short_stun"), EVeyraStatusKind::Stun, 0.0, BelowTheFloor))));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, TestStatus(TEXT("drag"), EVeyraStatusKind::MoveSpeed, -0.3, LongStun))));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Duration(TEXT("long_stun")), LongStun * Retained, 1e-4)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Duration(TEXT("floor_stun")), TestTenacityFloorSeconds, 1e-4)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Duration(TEXT("short_stun")), BelowTheFloor, 1e-4), TEXT("Tenacity never lengthens")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Duration(TEXT("drag")), LongStun, 1e-4), TEXT("a speed change is not crowd control")));
		}

		TEST_METHOD(AStunBlocksEveryActionAndInterrupts)
		{
			int32 Interruptions = 0;
			StatusLedger->OnInterrupted.AddLambda([&Interruptions] { ++Interruptions; });
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, TestStatus(TEXT("mire"), EVeyraStatusKind::Slow, 0.2, LongSeconds))));
			ASSERT_THAT(AreEqual(0, Interruptions, TEXT("a Slow does not interrupt")));
			ASSERT_THAT(IsTrue(VeyraCombat::GetActionBlocks(*Unit) == EVeyraActionBlocks::None));

			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, TestStatus(TEXT("daze"), EVeyraStatusKind::Stun, 0.0, LongSeconds))));
			ASSERT_THAT(AreEqual(1, Interruptions));
			ASSERT_THAT(IsTrue(VeyraCombat::GetActionBlocks(*Unit) == (EVeyraActionBlocks::Move | EVeyraActionBlocks::Attack | EVeyraActionBlocks::Cast)));
			ASSERT_THAT(IsTrue(VeyraCombat::RemoveStatus(*Unit, FVeyraContentId::FromText(TEXT("daze")).GetValue())));
			ASSERT_THAT(IsTrue(VeyraCombat::GetActionBlocks(*Unit) == EVeyraActionBlocks::None));
		}

		TEST_METHOD(RemovingAStatusRestoresItsStat)
		{
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, TestStatus(TEXT("anchor"), EVeyraStatusKind::DisplacementResistance, 0.5, LongSeconds))));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Value(UVeyraDefenceSet::GetDisplacementRetainedAttribute()), 0.5, 1e-5)));
			ASSERT_THAT(IsTrue(VeyraCombat::RemoveStatus(*Unit, FVeyraContentId::FromText(TEXT("anchor")).GetValue())));
			ASSERT_THAT(IsTrue(Value(UVeyraDefenceSet::GetDisplacementRetainedAttribute()) == 1.0));
			ASSERT_THAT(IsTrue(StatusLedger->GetLedger().Entries.IsEmpty()));
			ASSERT_THAT(IsFalse(VeyraCombat::RemoveStatus(*Unit, FVeyraContentId::FromText(TEXT("anchor")).GetValue())));
		}

		TEST_METHOD(DeathEndsEveryStatus)
		{
			constexpr double LethalDamage = 100000.0;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, TestStatus(TEXT("daze"), EVeyraStatusKind::Stun, 0.0, LongSeconds))));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, TestStatus(TEXT("haste"), EVeyraStatusKind::MoveSpeed, 0.2, LongSeconds))));
			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ EVeyraDamageType::TrueDamage, LethalDamage });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Caster, *Unit, Damage)));
			ASSERT_THAT(IsTrue(StatusLedger->GetLedger().Entries.IsEmpty()));
			ASSERT_THAT(IsTrue(Value(UVeyraMobilitySet::GetMoveSpeedAttribute()) == ExampleStats().MoveSpeed));
			ASSERT_THAT(IsFalse(VeyraCombat::ApplyStatus(*Caster, *Unit, TestStatus(TEXT("daze"), EVeyraStatusKind::Stun, 0.0, LongSeconds)),
				TEXT("a unit whose death is final takes no statuses")));
		}

		TEST_METHOD(RefusesStatusesOutsideTheRules)
		{
			const FVeyraStatusSpec Refused[] = {
				TestStatus(TEXT("bad_stun"), EVeyraStatusKind::Stun, 0.5, LongSeconds),
				TestStatus(TEXT("bad_slow"), EVeyraStatusKind::Slow, 1.0, LongSeconds),
				TestStatus(TEXT("bad_duration"), EVeyraStatusKind::Slow, 0.2, 0.0),
				TestStatus(TEXT("bad_haste"), EVeyraStatusKind::MoveSpeed, -1.0, LongSeconds),
				TestStatus(TEXT("bad_stacks"), EVeyraStatusKind::Tenacity, 0.4, LongSeconds, EVeyraStackingPolicy::Stacking, 3),
				TestStatus(TEXT("bad_unique"), EVeyraStatusKind::Tenacity, 0.1, LongSeconds, EVeyraStackingPolicy::UniqueRefresh, 2),
				TestStatus(TEXT("bad_cc_stack"), EVeyraStatusKind::Stun, 0.0, LongSeconds, EVeyraStackingPolicy::Stacking, 2),
			};
			// Each refused record, and the second kind for one ID.
			const int32 ExpectedRefusals = static_cast<int32>(UE_ARRAY_COUNT(Refused)) + 1;
			TestRunner->AddExpectedMessagePlain(TEXT("Refused status"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, ExpectedRefusals);
			for (const FVeyraStatusSpec& Spec : Refused)
			{
				ASSERT_THAT(IsFalse(VeyraCombat::ApplyStatus(*Caster, *Unit, Spec), Spec.Id.ToString()));
			}
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, TestStatus(TEXT("mire"), EVeyraStatusKind::Slow, 0.2, LongSeconds))));
			ASSERT_THAT(IsFalse(VeyraCombat::ApplyStatus(*Caster, *Unit, TestStatus(TEXT("mire"), EVeyraStatusKind::MoveSpeed, 0.2, LongSeconds)),
				TEXT("one ID is one status")));
			ASSERT_THAT(AreEqual(1, StatusLedger->GetLedger().Entries.Num()));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
