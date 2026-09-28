// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Brain/VeyraBotLane.h"
#include "Brain/VeyraBotRules.h"
#include "Components/ActorTestSpawner.h"
#include "CQTest.h"
#include "Inventory/VeyraInventoryRules.h"
#include "Tests/Items/VeyraItemsTestCatalog.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraBotsTests
{
	using VeyraItemsTests::ItemId;
	using VeyraItemsTests::TestCatalog;

	// Veyra.Bots.BotRules.*: how a bot decides, one priority at a time (ADR-013 §4, §8), as pure rules.
	TEST_CLASS(BotRules, "Veyra.Bots")
	{
		// Fixture values.
		static constexpr double Far = 5000.0;
		static constexpr double Near = 300.0;
		static constexpr double UnitRadius = 40.0;
		static constexpr double Seed = 7;

		FActorTestSpawner Spawner;
		FVeyraBotsTuning Tuning;
		FVeyraBotDifficultyTuning Difficulty;
		FVeyraBotMemory Memory;
		FRandomStream Random{ static_cast<int32>(Seed) };

		BEFORE_EACH()
		{
			Tuning.Senses.SightRadius = 1400.0;
			Tuning.Senses.SafeRadius = 1600.0;
			Tuning.Senses.TowerMargin = 200.0;
			Tuning.Positioning.FollowDistance = 350.0;
			Tuning.Positioning.HoldTolerance = 150.0;
			Tuning.Positioning.LeaveFountainHealthFraction = 0.9;
			Difficulty.ThinkSeconds = 0.25;
			Difficulty.ReactionSeconds = 0.5;
			Difficulty.LastHitChance = 1.0;
			Difficulty.CastChance = 1.0;
			Difficulty.RetreatHealthFraction = 0.3;
			Difficulty.FightHealthMargin = 0.0;
		}

		/** A unit at X along the ground, at Fraction of its Health; each call is a distinct actor, which the rules only compare. */
		FVeyraBotUnit Unit(double X, double Fraction = 1.0)
		{
			static constexpr double MaxHealth = 1000.0;
			FVeyraBotUnit Out;
			Out.Actor = &Spawner.SpawnActor<AActor>();
			Out.Location = FVector(X, 0.0, 0.0);
			Out.MaxHealth = MaxHealth;
			Out.Health = MaxHealth * Fraction;
			Out.Radius = UnitRadius;
			return Out;
		}

		FVeyraBotView AliveAt(double X, double Fraction = 1.0)
		{
			FVeyraBotView View;
			View.bAlive = true;
			View.Self = Unit(X, Fraction);
			View.AttackRange = 150.0;
			View.AttackDamage = 60.0;
			View.Home = FVector(-Far, 0.0, 0.0);
			View.LaneHold = View.Self.Location;
			return View;
		}

		FVeyraBotIntent Decide(const FVeyraBotView& View)
		{
			return VeyraBotRules::Decide(View, Difficulty, Tuning, Memory, Random);
		}

		static FVeyraBotSlot Slot(EVeyraAbilitySlot Which, EVeyraBotAbilityUse Use, EVeyraBotTargeting Targeting, double Reach)
		{
			FVeyraBotSlot Out;
			Out.Slot = Which;
			Out.Use = Use;
			Out.Profile.Targeting = Targeting;
			Out.Profile.Reach = Reach;
			Out.bReady = true;
			return Out;
		}

		TEST_METHOD(TheDeadAndTheRecallingWait)
		{
			FVeyraBotView View;
			ASSERT_THAT(IsTrue(Decide(View).Action == EVeyraBotAction::Wait));
			View = AliveAt(0.0);
			View.bRecalling = true;
			ASSERT_THAT(IsTrue(Decide(View).Action == EVeyraBotAction::Wait));
		}

		TEST_METHOD(AHurtBotRetreatsThenRecallsOnceSafeAndHealsBeforeLeaving)
		{
			FVeyraBotView View = AliveAt(0.0, 0.2);
			View.EnemyVanguards.Add(Unit(Near));
			const FVeyraBotIntent Retreat = Decide(View);
			ASSERT_THAT(IsTrue(Retreat.Action == EVeyraBotAction::Retreat && Retreat.Destination == View.Home));

			View.EnemyVanguards.Reset();
			ASSERT_THAT(IsTrue(Decide(View).Action == EVeyraBotAction::Recall, TEXT("no enemy near: recall")));

			// Home, it heals before going back, and goes back once healed.
			View.bAtFountain = true;
			View.Self.Health = View.Self.MaxHealth * 0.5;
			ASSERT_THAT(IsTrue(Decide(View).Action == EVeyraBotAction::Wait));
			View.Self.Health = View.Self.MaxHealth;
			View.LaneHold = FVector(Far, 0.0, 0.0);
			ASSERT_THAT(IsTrue(Decide(View).Action == EVeyraBotAction::Move && !Memory.bRetreating));
		}

		TEST_METHOD(AnEscapeIsCastAtTheThreatWithoutAnAttackAfter)
		{
			FVeyraBotView View = AliveAt(0.0, 0.2);
			View.EnemyVanguards.Add(Unit(Near));
			View.Slots.Add(Slot(EVeyraAbilitySlot::E, EVeyraBotAbilityUse::Escape, EVeyraBotTargeting::Point, Near * 2.0));
			const FVeyraBotIntent Intent = Decide(View);
			ASSERT_THAT(IsTrue(Intent.Action == EVeyraBotAction::Cast && Intent.Slot == EVeyraAbilitySlot::E && !Intent.Target.IsValid()));
			ASSERT_THAT(IsTrue(Intent.CastTarget.bHasLocation && Intent.CastTarget.Location == View.EnemyVanguards[0].Location));
		}

		TEST_METHOD(ABotBacksOutOfATowerShootingIt)
		{
			FVeyraBotView View = AliveAt(0.0);
			View.LaneHold = FVector(-Far, 0.0, 0.0);
			FVeyraBotStructure& Tower = View.EnemyStructure.Emplace();
			Tower.Unit = Unit(Near);
			Tower.AttackRange = 750.0;
			Tower.bTargetsBot = true;
			const FVeyraBotIntent Intent = Decide(View);
			ASSERT_THAT(IsTrue(Intent.Action == EVeyraBotAction::Retreat && Intent.Destination == View.LaneHold));
		}

		TEST_METHOD(ItFightsOnlyAfterWatchingAnEnemyForItsReactionTime)
		{
			FVeyraBotView View = AliveAt(0.0);
			View.EnemyVanguards.Add(Unit(Near, 0.5));
			View.Now = 10.0;
			ASSERT_THAT(IsTrue(Decide(View).Action != EVeyraBotAction::Attack, TEXT("just seen")));
			View.Now += Difficulty.ReactionSeconds;
			const FVeyraBotIntent Intent = Decide(View);
			ASSERT_THAT(IsTrue(Intent.Action == EVeyraBotAction::Attack && Intent.Target == View.EnemyVanguards[0].Actor));
		}

		TEST_METHOD(ItPicksTheWeakestFoeAndCastsTheUltimateFirst)
		{
			Difficulty.ReactionSeconds = 0.0;
			FVeyraBotView View = AliveAt(0.0);
			View.EnemyVanguards.Add(Unit(Near, 0.8));
			View.EnemyVanguards.Add(Unit(Near, 0.4));
			View.Slots.Add(Slot(EVeyraAbilitySlot::Q, EVeyraBotAbilityUse::Damage, EVeyraBotTargeting::Unit, Near * 2.0));
			View.Slots.Add(Slot(EVeyraAbilitySlot::R, EVeyraBotAbilityUse::Damage, EVeyraBotTargeting::Point, Near * 2.0));
			const FVeyraBotIntent Intent = Decide(View);
			ASSERT_THAT(IsTrue(Intent.Action == EVeyraBotAction::Cast && Intent.Slot == EVeyraAbilitySlot::R));
			ASSERT_THAT(IsTrue(Intent.Target == View.EnemyVanguards[1].Actor, TEXT("the weaker foe")));

			// Out of the ability's reach it attacks instead; with no chance to cast, it attacks too.
			View.Slots.Reset();
			View.Slots.Add(Slot(EVeyraAbilitySlot::Q, EVeyraBotAbilityUse::Damage, EVeyraBotTargeting::Unit, UnitRadius));
			ASSERT_THAT(IsTrue(Decide(View).Action == EVeyraBotAction::Attack));
		}

		TEST_METHOD(ItNeverFightsAHealthierFoeOrOneUnderAnEnemyTower)
		{
			Difficulty.ReactionSeconds = 0.0;
			Difficulty.FightHealthMargin = 0.1;
			FVeyraBotView View = AliveAt(0.0, 0.5);
			View.EnemyVanguards.Add(Unit(Near, 0.45));
			ASSERT_THAT(IsTrue(Decide(View).Action != EVeyraBotAction::Attack, TEXT("within the margin")));

			View = AliveAt(0.0);
			View.EnemyVanguards.Add(Unit(Near, 0.2));
			FVeyraBotStructure& Tower = View.EnemyStructure.Emplace();
			Tower.Unit = Unit(Near * 2.0);
			Tower.AttackRange = 750.0;
			ASSERT_THAT(IsTrue(Decide(View).Action != EVeyraBotAction::Attack, TEXT("no tower dives")));
		}

		TEST_METHOD(ItLastHitsAFluxbornOneAttackKillsAndSiegesWithItsWave)
		{
			FVeyraBotView View = AliveAt(0.0);
			FVeyraBotUnit Healthy = Unit(Near);
			FVeyraBotUnit Low = Unit(Near);
			Low.Health = View.AttackDamage * 0.5;
			View.EnemyFluxborn = { Healthy, Low };
			FVeyraBotIntent Intent = Decide(View);
			ASSERT_THAT(IsTrue(Intent.Action == EVeyraBotAction::Attack && Intent.Target == Low.Actor));

			// Armor that halves the attack keeps a Fluxborn just out of reach.
			View.EnemyFluxborn[1].Health = View.AttackDamage * 0.75;
			View.EnemyFluxborn[1].DamageTaken = 0.5;
			ASSERT_THAT(IsTrue(Decide(View).Action != EVeyraBotAction::Attack));

			FVeyraBotStructure& Tower = View.EnemyStructure.Emplace();
			Tower.Unit = Unit(Far);
			Tower.bVulnerable = true;
			Tower.bAlliesInRange = true;
			Intent = Decide(View);
			ASSERT_THAT(IsTrue(Intent.Action == EVeyraBotAction::Attack && Intent.Target == Tower.Unit.Actor));
		}

		TEST_METHOD(OtherwiseItTakesItsPlaceInLane)
		{
			FVeyraBotView View = AliveAt(0.0);
			View.LaneHold = FVector(Far, 0.0, 0.0);
			const FVeyraBotIntent Intent = Decide(View);
			ASSERT_THAT(IsTrue(Intent.Action == EVeyraBotAction::Move && Intent.Destination == View.LaneHold));
			View.LaneHold = FVector(Tuning.Positioning.HoldTolerance / 2.0, 0.0, 0.0);
			ASSERT_THAT(IsTrue(Decide(View).Action == EVeyraBotAction::Wait));
		}

		TEST_METHOD(ALeadingAimFallsWhereTheTargetWillBe)
		{
			FVeyraBotUnit Caster = Unit(0.0);
			FVeyraBotUnit Target = Unit(1000.0);
			Target.Velocity = FVector(0.0, 300.0, 0.0);
			FVeyraBotAbilityProfile Profile;
			Profile.LeadSeconds = 0.5;
			Profile.ProjectileSpeed = 2000.0;
			ASSERT_THAT(IsTrue(VeyraBotRules::AimAt(Caster, Target, Profile, EVeyraBotAim::AtTarget) == Target.Location));
			// Half a second of windup and half a second of flight: a second of walking.
			ASSERT_THAT(IsTrue(VeyraBotRules::AimAt(Caster, Target, Profile, EVeyraBotAim::Lead).Equals(FVector(1000.0, 300.0, 0.0))));
		}

		TEST_METHOD(ItRanksTheUltimateWheneverItMayThenByPriority)
		{
			const TArray<EVeyraBotSkill> Priority = { EVeyraBotSkill::E, EVeyraBotSkill::Q, EVeyraBotSkill::W };
			TSet<EVeyraAbilitySlot> Open = { EVeyraAbilitySlot::Q, EVeyraAbilitySlot::W };
			const auto CanRank = [&Open](EVeyraAbilitySlot Slot) { return Open.Contains(Slot); };
			ASSERT_THAT(IsTrue(VeyraBotRules::NextRank(Priority, CanRank) == EVeyraAbilitySlot::Q, TEXT("E is full, so Q")));
			Open.Add(EVeyraAbilitySlot::R);
			ASSERT_THAT(IsTrue(VeyraBotRules::NextRank(Priority, CanRank) == EVeyraAbilitySlot::R));
			Open.Reset();
			ASSERT_THAT(IsFalse(VeyraBotRules::NextRank(Priority, CanRank).IsSet()));
		}

		TEST_METHOD(ItBuysTheNextItemOrTheDearestPartItAffords)
		{
			const FVeyraItemsTuning Items = TestCatalog();
			TArray<FVeyraInventorySlot> Slots;
			Slots.SetNum(Items.Shop.InventorySlots);
			const TArray<FVeyraContentId> Build = { ItemId(TEXT("test_temper")), ItemId(TEXT("test_wheel")) };
			// Temper wants a harness (plate and grip) and a plate; 400 buys a plate, the dearest part.
			ASSERT_THAT(IsTrue(VeyraBotRules::NextPurchase(Items, Build, Slots, {}, 400.0) == ItemId(TEXT("test_plate"))));
			ASSERT_THAT(IsTrue(VeyraBotRules::NextPurchase(Items, Build, Slots, {}, 360.0) == ItemId(TEXT("test_grip"))));
			// Too poor for any part: it saves, rather than skip to the wheel.
			ASSERT_THAT(IsFalse(VeyraBotRules::NextPurchase(Items, Build, Slots, {}, 100.0).IsSet()));
			// Holding the temper, it moves on.
			Slots[0].Item = ItemId(TEXT("test_temper"));
			Slots[0].Count = 1;
			ASSERT_THAT(IsTrue(VeyraBotRules::NextPurchase(Items, Build, Slots, {}, 5000.0) == ItemId(TEXT("test_wheel"))));
		}

		TEST_METHOD(TheLaneMeasuresDistanceAlongItsPath)
		{
			const TArray<FVector2D> Path = { FVector2D(0.0, 0.0), FVector2D(1000.0, 0.0), FVector2D(1000.0, 1000.0) };
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraBotLane::DistanceAlong(Path, FVector2D(500.0, 50.0)), 500.0)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraBotLane::DistanceAlong(Path, FVector2D(1100.0, 400.0)), 1400.0)));
			ASSERT_THAT(IsTrue(VeyraBotLane::PointAt(Path, 1500.0).Equals(FVector2D(1000.0, 500.0))));
			ASSERT_THAT(IsTrue(VeyraBotLane::PointAt(Path, 5000.0).Equals(FVector2D(1000.0, 1000.0)), TEXT("clamped to its end")));

			// Behind its wave, but never inside the enemy tower's reach unless its wave holds the tower.
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraBotLane::HoldDistance(2000.0, 500.0, {}, false, 350.0), 1650.0)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraBotLane::HoldDistance(2000.0, 500.0, 1200.0, false, 350.0), 1200.0)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraBotLane::HoldDistance(2000.0, 500.0, 1200.0, true, 350.0), 1650.0)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraBotLane::HoldDistance({}, 500.0, {}, false, 350.0), 500.0), TEXT("no wave: at its structure")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
