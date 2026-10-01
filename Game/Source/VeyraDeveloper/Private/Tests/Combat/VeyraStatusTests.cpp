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
		static constexpr float WorldStep = 0.1f;
		static constexpr double CadencePerStack = 0.1;
		static constexpr double CadenceSeconds = 1.0;
		static constexpr double CadenceDecaySeconds = 0.5;
		static constexpr int32 CadenceStacks = 3;
		static constexpr double Reach = 150.0;
		static constexpr double Half = 0.5;

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

		TEST_METHOD(ARootStopsMovementAndMovingCastsButNotAttacksOrCasts)
		{
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, TestStatus(TEXT("root"), EVeyraStatusKind::Root, 0.0, LongSeconds))));
			const EVeyraActionBlocks Blocks = VeyraCombat::GetActionBlocks(*Unit);
			ASSERT_THAT(IsTrue(EnumHasAllFlags(Blocks, EVeyraActionBlocks::Move | EVeyraActionBlocks::Dash), TEXT("no walking and no dashing (ADR-026 §3)")));
			ASSERT_THAT(IsFalse(EnumHasAnyFlags(Blocks, EVeyraActionBlocks::Attack | EVeyraActionBlocks::Cast), TEXT("it may attack and cast in place")));
			ASSERT_THAT(IsTrue(VeyraStatuses::IsCrowdControl(EVeyraStatusKind::Root) && VeyraStatuses::IsTenacityReducible(EVeyraStatusKind::Root)));
			ASSERT_THAT(IsFalse(VeyraStatuses::Validate(TestStatus(TEXT("root"), EVeyraStatusKind::Root, Half, LongSeconds)).IsEmpty(), TEXT("a root has no magnitude")));
		}

		TEST_METHOD(InvisibleAndUntargetableAreNotCrowdControlAndAnAttackEndsOnlyTheFirst)
		{
			for (const EVeyraStatusKind Kind : { EVeyraStatusKind::Invisible, EVeyraStatusKind::Untargetable })
			{
				ASSERT_THAT(IsFalse(VeyraStatuses::IsCrowdControl(Kind)));
				ASSERT_THAT(IsFalse(VeyraStatuses::Validate(TestStatus(TEXT("test_kind"), Kind, Half, LongSeconds)).IsEmpty(), TEXT("it has no magnitude")));
				ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Unit, *Unit, TestStatus(Kind == EVeyraStatusKind::Invisible ? TEXT("invisible") : TEXT("untargetable"), Kind, 0.0, LongSeconds))));
			}
			ASSERT_THAT(IsTrue(VeyraCombat::GetActionBlocks(*Unit) == EVeyraActionBlocks::None, TEXT("neither stops its holder acting")));
			// Attacking or an offensive cast ends stealth, of both grades (Combat Bible §11; ADR-030 §1).
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Unit, *Unit, TestStatus(TEXT("camouflage"), EVeyraStatusKind::Camouflage, Reach, LongSeconds))));
			VeyraCombat::EndStealth(*Unit);
			ASSERT_THAT(IsTrue(Find(TEXT("invisible")) == nullptr && Find(TEXT("camouflage")) == nullptr));
			ASSERT_THAT(IsTrue(Find(TEXT("untargetable")) != nullptr, TEXT("an Untargetable state is not stealth")));
		}

		TEST_METHOD(GroundingStopsMovingCastsAndBlindingIsCrowdControl)
		{
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, TestStatus(TEXT("grounded"), EVeyraStatusKind::Grounded, 0.0, LongSeconds))));
			const EVeyraActionBlocks Blocks = VeyraCombat::GetActionBlocks(*Unit);
			ASSERT_THAT(IsTrue(Blocks == EVeyraActionBlocks::Dash, TEXT("no dashing, and walking, attacking and casting stay (ADR-028 §2)")));
			for (const EVeyraStatusKind Kind : { EVeyraStatusKind::Grounded, EVeyraStatusKind::Blind })
			{
				ASSERT_THAT(IsTrue(VeyraStatuses::IsCrowdControl(Kind) && VeyraStatuses::IsTenacityReducible(Kind)));
				ASSERT_THAT(IsFalse(VeyraStatuses::Validate(TestStatus(TEXT("test_kind"), Kind, Half, LongSeconds)).IsEmpty(), TEXT("it has no magnitude")));
			}
		}

		TEST_METHOD(AStatusLandsOnlyOnTheKindsOfUnitItNames)
		{
			// The unit is a Vanguard's participant.
			FVeyraStatusSpec Splinter = TestStatus(TEXT("splinter"), EVeyraStatusKind::Counter, 0.0, LongSeconds);
			Splinter.LandsOn = { EVeyraUnitKind::Fluxborn };
			ASSERT_THAT(IsFalse(VeyraCombat::ApplyStatus(*Caster, *Unit, Splinter), TEXT("not on a Vanguard (ADR-026 §2)")));
			ASSERT_THAT(IsTrue(Find(TEXT("splinter")) == nullptr));
			Splinter.LandsOn = { EVeyraUnitKind::Vanguard };
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, Splinter)));
		}

		TEST_METHOD(AMobileAttackKeepsAShareOfSpeedAboveNoneUpToAll)
		{
			ASSERT_THAT(IsTrue(VeyraStatuses::Validate(TestStatus(TEXT("stride"), EVeyraStatusKind::MobileAttack, 1.0, LongSeconds)).IsEmpty()));
			ASSERT_THAT(IsFalse(VeyraStatuses::Validate(TestStatus(TEXT("stride"), EVeyraStatusKind::MobileAttack, 0.0, LongSeconds)).IsEmpty()));
			ASSERT_THAT(IsFalse(VeyraStatuses::Validate(TestStatus(TEXT("stride"), EVeyraStatusKind::MobileAttack, 1.5, LongSeconds)).IsEmpty()));
			ASSERT_THAT(IsFalse(VeyraStatuses::IsCrowdControl(EVeyraStatusKind::MobileAttack)));
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

		TEST_METHOD(ARefreshRenewsTheDurationAndRemovalStopsTheTicks)
		{
			constexpr double PerTick = 10.0;
			constexpr double TickSeconds = 1.0;
			constexpr double Lasts = 3.0;
			constexpr double Margin = 0.1;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, Burn(PerTick, TickSeconds, Lasts))));
			AdvanceTimers(TickSeconds + Margin);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HealthLost(), PerTick, 1e-3)));
			// Reapplied from the same source: its duration starts again (§14), its next tick still a second after the last.
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, Burn(PerTick, TickSeconds, Lasts))));
			AdvanceTimers(TickSeconds / 2.0);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HealthLost(), PerTick, 1e-3), TEXT("no tick half a second into the new application")));
			ASSERT_THAT(IsTrue(VeyraCombat::RemoveStatus(*Unit, FVeyraContentId::FromText(TEXT("burn")).GetValue())));
			AdvanceTimers(Lasts + Margin);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HealthLost(), PerTick, 1e-3), TEXT("removed early, it ticks no more")));
		}

		TEST_METHOD(ARefreshKeepsTheTickCadence)
		{
			constexpr double PerTick = 10.0;
			constexpr double TickSeconds = 1.0;
			constexpr double Lasts = 3.0;
			constexpr double Early = 0.6;
			constexpr double Margin = 0.1;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, Burn(PerTick, TickSeconds, Lasts))));
			AdvanceTimers(Early);
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, Burn(PerTick, TickSeconds, Lasts))));
			AdvanceTimers(TickSeconds - Early + Margin);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HealthLost(), PerTick, 1e-3), TEXT("the tick came when it would have (ADR-026 §7)")));
			// Refreshed before every tick is due, it still ticks once a second.
			for (int32 Refresh = 0; Refresh < 4; ++Refresh)
			{
				AdvanceTimers(Early);
				ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, Burn(PerTick, TickSeconds, Lasts))));
			}
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HealthLost(), 3.0 * PerTick, 1e-3), FString::SanitizeFloat(HealthLost())));
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

		TEST_METHOD(MagicResistReductionCutsTheRetainedResistanceUpToItsStacks)
		{
			// Fixture values: 5% a stack, three at most (ADR-023 §5).
			constexpr double PerStack = 0.05;
			constexpr int32 MaxStacks = 3;
			const FVeyraStatusSpec Shred = TestStatus(TEXT("shred"), EVeyraStatusKind::MagicResistReduction, PerStack, LongSeconds, EVeyraStackingPolicy::Stacking, MaxStacks);
			for (int32 Applied = 0; Applied <= MaxStacks; ++Applied)
			{
				ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, Shred)));
			}
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Value(UVeyraDefenceSet::GetMagicResistReductionRetainedAttribute()), 1.0 - PerStack * MaxStacks, 1e-5)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Value(UVeyraDefenceSet::GetArmorReductionRetainedAttribute()), 1.0), TEXT("Armor keeps all of itself")));
			ASSERT_THAT(IsFalse(VeyraStatuses::IsCrowdControl(EVeyraStatusKind::MagicResistReduction)));
			// Stacks that would remove all of it are refused.
			const FVeyraStatusSpec Whole = TestStatus(TEXT("shred"), EVeyraStatusKind::MagicResistReduction, 0.5, LongSeconds, EVeyraStackingPolicy::Stacking, 2);
			ASSERT_THAT(IsFalse(VeyraStatuses::Validate(Whole).IsEmpty()));
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

		/**
		 * Moves world time and its timers on by Seconds in small steps, so effects expire as the world's
		 * clock passes them. A time-only world tick skips timers, and Gameplay Effects expire by timer.
		 */
		void AdvanceWorld(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, WorldStep);
				++GFrameCounter;
				World.GetTimerManager().Tick(WorldStep);
			}
		}

		TEST_METHOD(AStackThatDecaysLosesOneAtATime)
		{
			FVeyraStatusSpec Cadence = TestStatus(TEXT("test_cadence"), EVeyraStatusKind::AttackSpeed, CadencePerStack, CadenceSeconds, EVeyraStackingPolicy::Stacking, CadenceStacks);
			Cadence.StackDecaySeconds = CadenceDecaySeconds;
			const double Base = Value(UVeyraOffenceSet::GetAttackSpeedAttribute());
			for (int32 Stack = 0; Stack < CadenceStacks; ++Stack)
			{
				ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, Cadence)));
			}
			ASSERT_THAT(AreEqual(Find(TEXT("test_cadence"))->Stacks, CadenceStacks));
			// Its time runs out: one stack goes, and the rest last the decay time again (ADR-018 §2).
			AdvanceWorld(CadenceSeconds + WorldStep);
			ASSERT_THAT(IsNotNull(Find(TEXT("test_cadence")), TEXT("one stack goes, not the status")));
			ASSERT_THAT(AreEqual(Find(TEXT("test_cadence"))->Stacks, CadenceStacks - 1));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Value(UVeyraOffenceSet::GetAttackSpeedAttribute()), Base * (1.0 + CadencePerStack * (CadenceStacks - 1)), 1e-4)));
			AdvanceWorld(CadenceDecaySeconds + WorldStep);
			ASSERT_THAT(IsTrue(Find(TEXT("test_cadence")) && Find(TEXT("test_cadence"))->Stacks == CadenceStacks - 2));
			AdvanceWorld(CadenceDecaySeconds + WorldStep);
			ASSERT_THAT(IsNull(Find(TEXT("test_cadence")), TEXT("the last stack goes")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Value(UVeyraOffenceSet::GetAttackSpeedAttribute()), Base, 1e-4)));

			// Removed early, it ends at once, whatever it had.
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, Cadence) && VeyraCombat::ApplyStatus(*Caster, *Unit, Cadence)));
			ASSERT_THAT(IsTrue(VeyraCombat::RemoveStatus(*Unit, Cadence.Id) && !Find(TEXT("test_cadence"))));
		}

		TEST_METHOD(PlantedStopsMovementButNotAttacksOrCasts)
		{
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Unit, *Unit, TestStatus(TEXT("test_planted"), EVeyraStatusKind::Planted, 0.0, LongSeconds))));
			ASSERT_THAT(IsTrue(StatusLedger->GetActionBlocks() == EVeyraActionBlocks::Move));
			// A choice, not crowd control: Tenacity leaves it whole.
			ASSERT_THAT(IsFalse(VeyraStatuses::IsTenacityReducible(EVeyraStatusKind::Planted)));
		}

		TEST_METHOD(TotalsAndRetainedCombineTheirEntries)
		{
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, TestStatus(TEXT("test_reach"), EVeyraStatusKind::AttackRange, Reach, LongSeconds))));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Unit, *Unit, TestStatus(TEXT("test_reach_2"), EVeyraStatusKind::AttackRange, Reach, LongSeconds))));
			ASSERT_THAT(IsTrue(StatusLedger->GetTotal(EVeyraStatusKind::AttackRange) == Reach * 2.0, TEXT("every range adds")));
			ASSERT_THAT(IsTrue(StatusLedger->GetTotalFrom(EVeyraStatusKind::AttackRange, *Caster) == Reach, TEXT("one source's own")));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Unit, *Unit, TestStatus(TEXT("test_resist"), EVeyraStatusKind::SlowResistance, Half, LongSeconds))));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, TestStatus(TEXT("test_resist_2"), EVeyraStatusKind::SlowResistance, Half, LongSeconds))));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(StatusLedger->GetRetained(EVeyraStatusKind::SlowResistance), Half * Half), TEXT("resistances multiply, as Tenacity does")));
		}

		TEST_METHOD(TheNewKindsKeepTheirMagnitudesInRange)
		{
			const auto Valid = [](EVeyraStatusKind Kind, double Magnitude, EVeyraStackingPolicy Stacking = EVeyraStackingPolicy::UniqueRefresh, int32 MaxStacks = 1,
								  double Decay = 0.0) {
				FVeyraStatusSpec Spec = TestStatus(TEXT("test_kind"), Kind, Magnitude, LongSeconds, Stacking, MaxStacks);
				Spec.StackDecaySeconds = Decay;
				return VeyraStatuses::Validate(Spec).IsEmpty();
			};
			ASSERT_THAT(IsTrue(Valid(EVeyraStatusKind::AttackRange, Reach) && !Valid(EVeyraStatusKind::AttackRange, 0.0)));
			ASSERT_THAT(IsTrue(Valid(EVeyraStatusKind::SourceAttackRange, Reach) && !Valid(EVeyraStatusKind::SourceAttackRange, -Reach)));
			ASSERT_THAT(IsTrue(Valid(EVeyraStatusKind::AttackSpeedCap, 3.0) && !Valid(EVeyraStatusKind::AttackSpeedCap, 3.0, EVeyraStackingPolicy::Stacking, 2)));
			ASSERT_THAT(IsTrue(Valid(EVeyraStatusKind::SlowResistance, Half) && !Valid(EVeyraStatusKind::SlowResistance, 1.0)));
			ASSERT_THAT(IsTrue(Valid(EVeyraStatusKind::Planted, 0.0) && !Valid(EVeyraStatusKind::Planted, Half)));
			ASSERT_THAT(IsTrue(Valid(EVeyraStatusKind::Camouflage, 400.0) && !Valid(EVeyraStatusKind::Camouflage, 0.0)));
			// Only a stacking status decays a stack at a time.
			ASSERT_THAT(IsTrue(Valid(EVeyraStatusKind::AttackSpeed, CadencePerStack, EVeyraStackingPolicy::Stacking, CadenceStacks, CadenceDecaySeconds)));
			ASSERT_THAT(IsFalse(Valid(EVeyraStatusKind::AttackSpeed, CadencePerStack, EVeyraStackingPolicy::UniqueRefresh, 1, CadenceDecaySeconds)));
			ASSERT_THAT(IsFalse(Valid(EVeyraStatusKind::AttackSpeed, CadencePerStack, EVeyraStackingPolicy::Stacking, CadenceStacks, -1.0)));
		}

		/** Caster and Unit on opposing sides, as the refusals of an enemy's crowd control need. */
		void MakeEnemies() const
		{
			CastChecked<AVeyraPlayerState>(Caster->GetOwner())->SetVeyraTeam(EVeyraTeam::A);
			CastChecked<AVeyraPlayerState>(Unit->GetOwner())->SetVeyraTeam(EVeyraTeam::B);
		}

		TEST_METHOD(UnstoppableRefusesAnEnemysCrowdControlButNotABuff)
		{
			MakeEnemies();
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Unit, *Unit, TestStatus(TEXT("test_unstoppable"), EVeyraStatusKind::Unstoppable, 0.0, LongSeconds))));
			ASSERT_THAT(IsFalse(VeyraCombat::ApplyStatus(*Caster, *Unit, TestStatus(TEXT("test_stun"), EVeyraStatusKind::Stun, 0.0, LongSeconds))));
			ASSERT_THAT(IsFalse(VeyraCombat::ApplyStatus(*Caster, *Unit, TestStatus(TEXT("test_slow"), EVeyraStatusKind::Slow, Half, LongSeconds))));
			ASSERT_THAT(IsFalse(VeyraCombat::ApplyStatus(*Caster, *Unit, TestStatus(TEXT("test_fear"), EVeyraStatusKind::Fear, Half, LongSeconds))));
			ASSERT_THAT(IsFalse(VeyraCombat::ApplyStatus(*Caster, *Unit, TestStatus(TEXT("test_knockup"), EVeyraStatusKind::Knockup, 0.0, LongSeconds))));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, TestStatus(TEXT("test_weaken"), EVeyraStatusKind::Weaken, Half, LongSeconds)), TEXT("a debuff that is no crowd control still lands")));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Unit, *Unit, TestStatus(TEXT("test_haste"), EVeyraStatusKind::MoveSpeed, Half, LongSeconds))));
		}

		TEST_METHOD(ImmunityToDisplacementRefusesAKnockupButNotAStun)
		{
			MakeEnemies();
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Unit, *Unit, TestStatus(TEXT("test_anchor"), EVeyraStatusKind::DisplacementImmunity, 0.0, LongSeconds))));
			ASSERT_THAT(IsFalse(VeyraCombat::ApplyStatus(*Caster, *Unit, TestStatus(TEXT("test_knockup"), EVeyraStatusKind::Knockup, 0.0, LongSeconds))));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Caster, *Unit, TestStatus(TEXT("test_stun"), EVeyraStatusKind::Stun, 0.0, LongSeconds))));
		}

		TEST_METHOD(DormantFearAndKnockupTakeEveryActionButOnlyFearIsShortened)
		{
			const EVeyraActionBlocks All = EVeyraActionBlocks::Move | EVeyraActionBlocks::Attack | EVeyraActionBlocks::Cast;
			for (const EVeyraStatusKind Kind : { EVeyraStatusKind::Dormant, EVeyraStatusKind::Fear, EVeyraStatusKind::Knockup })
			{
				const FVeyraStatusSpec Spec = TestStatus(TEXT("test_hold"), Kind, 0.0, LongSeconds);
				ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Unit, *Unit, Spec)));
				ASSERT_THAT(IsTrue(StatusLedger->GetActionBlocks() == All));
				ASSERT_THAT(IsTrue(VeyraCombat::RemoveStatus(*Unit, Spec.Id)));
			}
			ASSERT_THAT(IsTrue(VeyraStatuses::IsTenacityReducible(EVeyraStatusKind::Fear) && !VeyraStatuses::IsTenacityReducible(EVeyraStatusKind::Knockup)));
			ASSERT_THAT(IsTrue(VeyraStatuses::IsCrowdControl(EVeyraStatusKind::Knockup) && !VeyraStatuses::IsCrowdControl(EVeyraStatusKind::Dormant)));
		}

		TEST_METHOD(ADirectionalReductionGuardsItsArcOnly)
		{
			constexpr double Guard = 0.6;
			constexpr double Arc = 90.0;
			FVeyraStatusSpec Spec = TestStatus(TEXT("test_guard"), EVeyraStatusKind::DirectionalDamageReduction, Guard, LongSeconds);
			Spec.ArcDegrees = Arc;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Unit, *Unit, Spec)));
			const FVector Facing = FVector::ForwardVector;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(StatusLedger->GetDirectionalRetained(Facing, FVector::ForwardVector), 1.0 - Guard)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(StatusLedger->GetDirectionalRetained(Facing, FVector::ForwardVector.RotateAngleAxis(Arc / 2.0 - 1.0, FVector::UpVector)), 1.0 - Guard)));
			ASSERT_THAT(IsTrue(StatusLedger->GetDirectionalRetained(Facing, FVector::ForwardVector.RotateAngleAxis(Arc / 2.0 + 1.0, FVector::UpVector)) == 1.0));
			ASSERT_THAT(IsTrue(StatusLedger->GetDirectionalRetained(Facing, FVector::BackwardVector) == 1.0));
		}

		TEST_METHOD(AnAttackAmplificationMayNameTheKindsItAmplifies)
		{
			FVeyraStatusSpec Hunt = TestStatus(TEXT("test_hunt"), EVeyraStatusKind::AttackDamageAmplification, Half, LongSeconds);
			Hunt.UnitKinds = { EVeyraUnitKind::Vanguard };
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Unit, *Unit, Hunt)));
			ASSERT_THAT(IsTrue(StatusLedger->GetAttackAmplification(EVeyraUnitKind::Vanguard) == Half));
			ASSERT_THAT(IsTrue(StatusLedger->GetAttackAmplification(EVeyraUnitKind::Fluxborn) == 0.0 && StatusLedger->GetAttackAmplification({}) == 0.0));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Unit, *Unit, TestStatus(TEXT("test_fury"), EVeyraStatusKind::AttackDamageAmplification, Half, LongSeconds))));
			ASSERT_THAT(IsTrue(StatusLedger->GetAttackAmplification(EVeyraUnitKind::Fluxborn) == Half, TEXT("one naming no kind amplifies against all")));
		}

		TEST_METHOD(TheSecondWaveKeepsItsFieldsToItsKinds)
		{
			const auto Valid = [](const FVeyraStatusSpec& Spec) { return VeyraStatuses::Validate(Spec).IsEmpty(); };
			FVeyraStatusSpec Guard = TestStatus(TEXT("test_kind"), EVeyraStatusKind::DirectionalDamageReduction, Half, LongSeconds);
			ASSERT_THAT(IsFalse(Valid(Guard), TEXT("a guard needs its arc")));
			Guard.ArcDegrees = 120.0;
			ASSERT_THAT(IsTrue(Valid(Guard)));
			FVeyraStatusSpec Stun = TestStatus(TEXT("test_kind"), EVeyraStatusKind::Stun, 0.0, LongSeconds);
			Stun.ArcDegrees = 120.0;
			ASSERT_THAT(IsFalse(Valid(Stun)));
			FVeyraStatusSpec Haste = TestStatus(TEXT("test_kind"), EVeyraStatusKind::MoveSpeed, Half, LongSeconds);
			Haste.UnitKinds = { EVeyraUnitKind::Vanguard };
			ASSERT_THAT(IsFalse(Valid(Haste), TEXT("only an attack amplification names kinds")));
			ASSERT_THAT(IsTrue(Valid(TestStatus(TEXT("test_kind"), EVeyraStatusKind::Fear, Half, LongSeconds))));
			ASSERT_THAT(IsFalse(Valid(TestStatus(TEXT("test_kind"), EVeyraStatusKind::Fear, 1.0, LongSeconds))));
			ASSERT_THAT(IsFalse(Valid(TestStatus(TEXT("test_kind"), EVeyraStatusKind::BodyScale, 1.5, LongSeconds, EVeyraStackingPolicy::Stacking, 2))));
			ASSERT_THAT(IsFalse(Valid(TestStatus(TEXT("test_kind"), EVeyraStatusKind::Knockup, Half, LongSeconds))));
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
