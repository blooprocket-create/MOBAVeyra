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
			Tuning.Positioning.PushRange = 500.0;
			Tuning.Positioning.WellRange = 4500.0;
			Tuning.Jungle.GankRange = 3000.0;
			Tuning.Jungle.GankHealthFraction = 0.5;
			Tuning.Jungle.WellRange = 7000.0;
			Difficulty.ThinkSeconds = 0.25;
			Difficulty.ReactionSeconds = 0.5;
			Difficulty.LastHitChance = 1.0;
			Difficulty.LastHitLead = 1.0;
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
			// A dash that carries its caster away from its point, as Bryn's Kickback does.
			FVeyraBotSlot Kickback = Slot(EVeyraAbilitySlot::E, EVeyraBotAbilityUse::Escape, EVeyraBotTargeting::Point, Near * 2.0);
			Kickback.Profile.bAwayFromPoint = true;
			View.Slots.Add(Kickback);
			const FVeyraBotIntent Intent = Decide(View);
			ASSERT_THAT(IsTrue(Intent.Action == EVeyraBotAction::Cast && Intent.Slot == EVeyraAbilitySlot::E && !Intent.Target.IsValid()));
			ASSERT_THAT(IsTrue(Intent.CastTarget.bHasLocation && Intent.CastTarget.Location == View.EnemyVanguards[0].Location));
		}

		TEST_METHOD(ABlinkEscapesHomewardAndAHealIsCastWhileRetreating)
		{
			// Fixture value: a Blink's distance.
			constexpr double BlinkDistance = 400.0;
			FVeyraBotView View = AliveAt(0.0, 0.2);
			View.EnemyVanguards.Add(Unit(Near));
			View.Slots.Add(Slot(EVeyraAbilitySlot::Spell1, EVeyraBotAbilityUse::Escape, EVeyraBotTargeting::Point, BlinkDistance));
			FVeyraBotIntent Intent = Decide(View);
			// Chased from away from home: a Blink at the threat would jump into it, so it blinks homeward.
			ASSERT_THAT(IsTrue(Intent.Action == EVeyraBotAction::Cast && Intent.Slot == EVeyraAbilitySlot::Spell1));
			ASSERT_THAT(IsTrue(Intent.CastTarget.Location.Equals(FVector(-BlinkDistance, 0.0, 0.0)), Intent.CastTarget.Location.ToString()));

			View.Slots.Reset();
			View.Slots.Add(Slot(EVeyraAbilitySlot::Spell2, EVeyraBotAbilityUse::Defend, EVeyraBotTargeting::Self, 0.0));
			Intent = Decide(View);
			ASSERT_THAT(IsTrue(Intent.Action == EVeyraBotAction::Cast && Intent.Slot == EVeyraAbilitySlot::Spell2, TEXT("a heal as it runs")));
		}

		TEST_METHOD(ASecureSpellFinishesTheLargestCreatureOrWellItWouldKill)
		{
			// Fixture values: a Smite's True damage and reach.
			constexpr double SmiteDamage = 600.0;
			constexpr double SmiteReach = 500.0;
			FVeyraBotView View = AliveAt(0.0);
			FVeyraBotSlot Smite = Slot(EVeyraAbilitySlot::Spell2, EVeyraBotAbilityUse::Secure, EVeyraBotTargeting::Unit, SmiteReach);
			Smite.Profile.Damage = SmiteDamage;
			Smite.Profile.bTrueDamage = true;
			View.Slots.Add(Smite);
			FVeyraBotCamp& Camp = View.Camps.AddDefaulted_GetRef();
			Camp.Creatures.Add(Unit(Near, 0.7));
			ASSERT_THAT(IsTrue(Decide(View).Action != EVeyraBotAction::Cast, TEXT("700 Health: the Smite would not finish it")));

			Camp.Creatures.Add(Unit(Near, 0.5));
			FVeyraBotUnit Well = Unit(Near, 0.1);
			Well.MaxHealth *= 4.0;
			Well.Health = SmiteDamage;
			View.Wells.Add(Well);
			const FVeyraBotIntent Intent = Decide(View);
			ASSERT_THAT(IsTrue(Intent.Action == EVeyraBotAction::Cast && Intent.Slot == EVeyraAbilitySlot::Spell2 && Intent.CastTarget.Actor == Well.Actor.Get(),
				TEXT("the Well first: a steal")));

			View.Wells[0].Location = FVector(SmiteReach * 2.0, 0.0, 0.0);
			ASSERT_THAT(IsTrue(Decide(View).CastTarget.Actor == View.Camps[0].Creatures[1].Actor.Get(), TEXT("out of reach: the creature it would finish")));
		}

		TEST_METHOD(ASpellForOtherKindsIsNeverCastAtAVanguard)
		{
			FVeyraBotView View = AliveAt(0.0);
			View.EnemyVanguards.Add(Unit(Near, 0.5));
			Memory.FirstSeen.Add(View.EnemyVanguards[0].Actor, -Far);
			FVeyraBotSlot Smite = Slot(EVeyraAbilitySlot::Spell2, EVeyraBotAbilityUse::Damage, EVeyraBotTargeting::Unit, Near * 2.0);
			Smite.Profile.TargetKinds = { EVeyraUnitKind::Wildlife, EVeyraUnitKind::Objective };
			View.Slots.Add(Smite);
			ASSERT_THAT(IsTrue(Decide(View).Action == EVeyraBotAction::Attack, TEXT("it fights with basic attacks")));
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

		TEST_METHOD(ItStartsNoFightTheEnemyWaveWouldAnswer)
		{
			Difficulty.ReactionSeconds = 0.0;
			Difficulty.FluxbornTolerance = 1;
			FVeyraBotView View = AliveAt(0.0);
			FVeyraBotUnit& Defended = View.EnemyVanguards.Add_GetRef(Unit(Near, 0.4));
			Defended.Defenders = 2;
			ASSERT_THAT(IsTrue(Decide(View).Action != EVeyraBotAction::Attack, TEXT("two of its Fluxborn would turn on the bot")));
			// Another foe, farther from its wave: that one it takes on, though the first is weaker.
			FVeyraBotUnit& Alone = View.EnemyVanguards.Add_GetRef(Unit(Near, 0.5));
			Alone.Defenders = 1;
			const FVeyraBotIntent Fight = Decide(View);
			ASSERT_THAT(IsTrue(Fight.Action == EVeyraBotAction::Attack && Fight.Target == Alone.Actor, TEXT("one it tolerates")));
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

			// Armor that halves the attack keeps a Fluxborn just out of reach, for a bot choosing afresh.
			Memory.FarmTarget.Reset();
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

		TEST_METHOD(ItRecallsToShopWithGoldToSpendWhenNoEnemyIsNear)
		{
			Difficulty.ShopRecallGold = 1000.0;
			FVeyraBotView View = AliveAt(0.0);
			View.Gold = Difficulty.ShopRecallGold;
			View.bPurchaseWaiting = true;
			ASSERT_THAT(IsTrue(Decide(View).Action == EVeyraBotAction::Recall));
			// An enemy near: it stays; nothing to buy, or too little Gold: it stays.
			View.EnemyVanguards.Add(Unit(Near, 1.0));
			ASSERT_THAT(IsTrue(Decide(View).Action != EVeyraBotAction::Recall));
			View.EnemyVanguards.Reset();
			View.bPurchaseWaiting = false;
			ASSERT_THAT(IsTrue(Decide(View).Action != EVeyraBotAction::Recall));
			View.bPurchaseWaiting = true;
			View.Gold = Difficulty.ShopRecallGold - 1.0;
			ASSERT_THAT(IsTrue(Decide(View).Action != EVeyraBotAction::Recall));
		}

		TEST_METHOD(ItGoesForALastHitEarlyAndPushesOtherwise)
		{
			FVeyraBotView View = AliveAt(0.0);
			FVeyraBotUnit Hurt = Unit(Near);
			Hurt.Health = View.AttackDamage * 1.4;
			FVeyraBotUnit Healthy = Unit(Near);
			View.EnemyFluxborn = { Healthy, Hurt };
			// Out of reach of one attack: no last hit, and without pushing it holds.
			Difficulty.PushChance = 0.0;
			ASSERT_THAT(IsTrue(Decide(View).Action != EVeyraBotAction::Attack));
			// A lead covers it.
			Difficulty.LastHitLead = 1.5;
			FVeyraBotIntent Intent = Decide(View);
			ASSERT_THAT(IsTrue(Intent.Action == EVeyraBotAction::Attack && Intent.Target == Hurt.Actor));
			// With no last hit to take, it pushes with the weakest Fluxborn near it.
			Difficulty.LastHitLead = 1.0;
			Difficulty.PushChance = 1.0;
			Intent = Decide(View);
			ASSERT_THAT(IsTrue(Intent.Action == EVeyraBotAction::Attack && Intent.Target == Hurt.Actor, TEXT("the weakest")));
			// Not beyond its push range.
			View.EnemyFluxborn = { Unit(Far) };
			ASSERT_THAT(IsTrue(Decide(View).Action != EVeyraBotAction::Attack));
		}

		TEST_METHOD(ItKeepsAttackingTheFluxbornItChose)
		{
			FVeyraBotView View = AliveAt(0.0);
			View.LaneHold = FVector(-Far, 0.0, 0.0);
			const FVeyraBotUnit Chosen = Unit(Near);
			View.EnemyFluxborn = { Chosen };
			Difficulty.PushChance = 1.0;
			ASSERT_THAT(IsTrue(Decide(View).Target == Chosen.Actor));
			// A weaker one comes near and no new push is chosen: it stays on the first, not walking
			// back to its place and throwing the windup away.
			FVeyraBotUnit Weaker = Unit(Near);
			Weaker.Health = Chosen.Health / 2.0;
			View.EnemyFluxborn = { Chosen, Weaker };
			Difficulty.PushChance = 0.0;
			const FVeyraBotIntent Kept = Decide(View);
			ASSERT_THAT(IsTrue(Kept.Action == EVeyraBotAction::Attack && Kept.Target == Chosen.Actor));
			// Once it is gone, it chooses afresh.
			View.EnemyFluxborn = { Weaker };
			ASSERT_THAT(IsTrue(Decide(View).Action == EVeyraBotAction::Move));
		}

		TEST_METHOD(ItTakesAnOpenWellInReachWhenNoEnemyIsNear)
		{
			FVeyraBotView View = AliveAt(0.0);
			View.LaneHold = FVector(-Far, 0.0, 0.0);
			const FVeyraBotUnit Well = Unit(Near);
			View.Wells = { Well };
			FVeyraBotIntent Intent = Decide(View);
			ASSERT_THAT(IsTrue(Intent.Action == EVeyraBotAction::Attack && Intent.Target == Well.Actor));
			// An enemy near: it leaves the Well alone.
			View.EnemyVanguards.Add(Unit(Near));
			Intent = Decide(View);
			ASSERT_THAT(IsFalse(Intent.Action == EVeyraBotAction::Attack && Intent.Target == Well.Actor));
			// Beyond a laner's reach, it stays in its lane; a jungler's reaches farther.
			View.EnemyVanguards.Reset();
			const FVeyraBotUnit Distant = Unit(Tuning.Positioning.WellRange * 1.2);
			View.Wells = { Distant };
			ASSERT_THAT(IsFalse(Decide(View).Target == Distant.Actor));
			View.bJungle = true;
			ASSERT_THAT(IsTrue(Decide(View).Target == Distant.Actor));
		}

		TEST_METHOD(AJunglerClearsItsNearestCampKeepingAtItsCreature)
		{
			FVeyraBotView View = AliveAt(0.0);
			View.bJungle = true;
			FVeyraBotCamp Distant;
			Distant.Center = FVector(Far, 0.0, 0.0);
			Distant.Creatures = { Unit(Far) };
			FVeyraBotCamp Close;
			Close.Center = FVector(Near, 0.0, 0.0);
			Close.Creatures = { Unit(Near, 0.5), Unit(Near, 1.0) };
			View.Camps = { Distant, Close };
			FVeyraBotIntent Intent = Decide(View);
			ASSERT_THAT(IsTrue(Intent.Action == EVeyraBotAction::Attack && Intent.Target == Close.Creatures[0].Actor, TEXT("the weakest of the nearest camp")));
			// Another becomes weaker: it keeps at the one it chose.
			View.Camps[1].Creatures[1].Health = 1.0;
			ASSERT_THAT(IsTrue(Decide(View).Target == Close.Creatures[0].Actor));
		}

		TEST_METHOD(AJunglerWaitsAtTheCampBackSoonest)
		{
			FVeyraBotView View = AliveAt(0.0);
			View.bJungle = true;
			FVeyraBotCamp Later;
			Later.Center = FVector(Far, 0.0, 0.0);
			Later.SpawnsAt = 50.0;
			FVeyraBotCamp Sooner;
			Sooner.Center = FVector(-Far, 0.0, 0.0);
			Sooner.SpawnsAt = 20.0;
			View.Camps = { Later, Sooner };
			const FVeyraBotIntent Walk = Decide(View);
			ASSERT_THAT(IsTrue(Walk.Action == EVeyraBotAction::Move && Walk.Destination.Equals(Sooner.Center)));
			View.Self.Location = Sooner.Center;
			ASSERT_THAT(IsTrue(Decide(View).Action == EVeyraBotAction::Wait));
		}

		TEST_METHOD(AJunglerGanksAHurtEnemyNear)
		{
			FVeyraBotView View = AliveAt(0.0);
			View.bJungle = true;
			FVeyraBotCamp Camp;
			Camp.Center = FVector(Near, 0.0, 0.0);
			Camp.Creatures = { Unit(Near) };
			View.Camps = { Camp };
			const FVeyraBotUnit Hurt = Unit(Far / 2.0, 0.3);
			View.GankTargets = { Hurt };
			const FVeyraBotIntent Gank = Decide(View);
			ASSERT_THAT(IsTrue(Gank.Action == EVeyraBotAction::Move && Gank.Destination.Equals(Hurt.Location)));
			// A healthy one is no gank: it clears its camp.
			View.GankTargets = { Unit(Far / 2.0, 0.9) };
			ASSERT_THAT(IsTrue(Decide(View).Target == Camp.Creatures[0].Actor));
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

		TEST_METHOD(ARecipesSecondCopyOfAPartIsStillWanted)
		{
			// The wheel takes two grips: holding one, the bot still buys the second.
			const FVeyraItemsTuning Items = TestCatalog();
			TArray<FVeyraInventorySlot> Slots;
			Slots.SetNum(Items.Shop.InventorySlots);
			Slots[0].Item = ItemId(TEXT("test_grip"));
			Slots[0].Count = 1;
			const TArray<FVeyraContentId> Build = { ItemId(TEXT("test_wheel")) };
			ASSERT_THAT(IsTrue(VeyraBotRules::NextPurchase(Items, Build, Slots, {}, 360.0) == ItemId(TEXT("test_grip"))));
			// Holding both, only the wheel's own cost remains.
			Slots[1] = Slots[0];
			ASSERT_THAT(IsFalse(VeyraBotRules::NextPurchase(Items, Build, Slots, {}, 360.0).IsSet()));
			ASSERT_THAT(IsTrue(VeyraBotRules::NextPurchase(Items, Build, Slots, {}, 400.0) == ItemId(TEXT("test_wheel"))));
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
