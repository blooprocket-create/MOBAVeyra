// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Brain/VeyraBotRules.h"

#include "Inventory/VeyraInventoryRules.h"
#include "Tuning/VeyraItemsTuning.h"

namespace VeyraBotRules
{
namespace
{
	bool Holds(TConstArrayView<FVeyraInventorySlot> Slots, TConstArrayView<FVeyraPendingPurchase> Queue, const FVeyraContentId& Item)
	{
		return Slots.ContainsByPredicate([&Item](const FVeyraInventorySlot& Slot) { return !Slot.IsEmpty() && Slot.Item == Item; })
			|| Queue.ContainsByPredicate([&Item](const FVeyraPendingPurchase& Entry) { return Entry.Item == Item; });
	}

	/** Refusals no amount of Gold overcomes. */
	bool NeverBuyable(EVeyraShopRefusal Refusal)
	{
		return Refusal == EVeyraShopRefusal::UnknownItem || Refusal == EVeyraShopRefusal::Unique || Refusal == EVeyraShopRefusal::BootsLimit
			|| Refusal == EVeyraShopRefusal::InventoryFull;
	}

	/** Each held item and queued purchase, by count: what a recipe's parts draw on, each copy once. */
	TMap<FVeyraContentId, int32> HeldCounts(TConstArrayView<FVeyraInventorySlot> Slots, TConstArrayView<FVeyraPendingPurchase> Queue)
	{
		TMap<FVeyraContentId, int32> Counts;
		for (const FVeyraInventorySlot& Slot : Slots)
		{
			if (!Slot.IsEmpty())
			{
				Counts.FindOrAdd(Slot.Item) += Slot.Count;
			}
		}
		for (const FVeyraPendingPurchase& Entry : Queue)
		{
			Counts.FindOrAdd(Entry.Item) += 1;
		}
		return Counts;
	}

	/**
	 * The dearest part of Item's recipe, at any depth, that Gold buys now and is not already held. Each
	 * held copy counts once, so a recipe that needs a part twice still wants the second (Unheld: the
	 * copies not yet spoken for).
	 */
	void FindAffordablePart(const FVeyraItemsTuning& Items, const FVeyraContentId& Item, TConstArrayView<FVeyraInventorySlot> Slots,
		TConstArrayView<FVeyraPendingPurchase> Queue, double Gold, TMap<FVeyraContentId, int32>& Unheld, TOptional<FVeyraContentId>& Best, double& BestPrice)
	{
		const FVeyraItemDefinition* Definition = Items.Items.Find(Item);
		if (!Definition)
		{
			return;
		}
		for (const FVeyraContentId& Part : Definition->Components)
		{
			if (int32* Copies = Unheld.Find(Part); Copies && *Copies > 0)
			{
				--*Copies;
				continue;
			}
			const FVeyraPurchaseQuote Quote = VeyraInventory::Quote(Items, Slots, Queue, Part);
			if (Quote.Refusal == EVeyraShopRefusal::None && Quote.Price <= Gold && Quote.Price > BestPrice)
			{
				Best = Part;
				BestPrice = Quote.Price;
			}
			FindAffordablePart(Items, Part, Slots, Queue, Gold, Unheld, Best, BestPrice);
		}
	}

	bool IsUltimate(EVeyraAbilitySlot Slot)
	{
		return Slot == EVeyraAbilitySlot::R;
	}

	EVeyraAbilitySlot SlotOf(EVeyraBotSkill Skill)
	{
		switch (Skill)
		{
		case EVeyraBotSkill::W:
			return EVeyraAbilitySlot::W;
		case EVeyraBotSkill::E:
			return EVeyraAbilitySlot::E;
		default:
			return EVeyraAbilitySlot::Q;
		}
	}

	const FVeyraBotUnit* Nearest(const FVeyraBotUnit& From, TConstArrayView<FVeyraBotUnit> Units)
	{
		const FVeyraBotUnit* Found = nullptr;
		double Best = TNumericLimits<double>::Max();
		for (const FVeyraBotUnit& Unit : Units)
		{
			const double Distance = EdgeDistance(From, Unit);
			if (Distance < Best)
			{
				Best = Distance;
				Found = &Unit;
			}
		}
		return Found;
	}

	/** Whether Unit stands where an enemy tower would shoot it, with the margin a careful bot keeps. */
	bool IsCoveredByTower(const FVeyraBotView& View, const FVeyraBotUnit& Unit, double Margin)
	{
		const TOptional<FVeyraBotStructure>& Tower = View.EnemyStructure;
		return Tower.IsSet() && Tower->AttackRange > 0.0 && EdgeDistance(Unit, Tower->Unit) <= Tower->AttackRange + Margin;
	}

	FVeyraBotIntent Intent(EVeyraBotAction Action, const TCHAR* Reason)
	{
		FVeyraBotIntent Out;
		Out.Action = Action;
		Out.Reason = Reason;
		return Out;
	}

	FVeyraBotIntent MoveTo(EVeyraBotAction Action, const FVector& Destination, const TCHAR* Reason)
	{
		FVeyraBotIntent Out = Intent(Action, Reason);
		Out.Destination = Destination;
		return Out;
	}

	FVeyraBotIntent AttackOf(const FVeyraBotUnit& Target, const TCHAR* Reason)
	{
		FVeyraBotIntent Out = Intent(EVeyraBotAction::Attack, Reason);
		Out.Target = Target.Actor;
		return Out;
	}

	FVeyraBotIntent CastOf(const FVeyraBotView& View, const FVeyraBotSlot& Slot, const FVeyraBotUnit& Target, EVeyraBotAim Aim, const TCHAR* Reason)
	{
		FVeyraBotIntent Out = Intent(EVeyraBotAction::Cast, Reason);
		Out.Slot = Slot.Slot;
		Out.Target = Target.Actor;
		switch (Slot.Profile.Targeting)
		{
		case EVeyraBotTargeting::Unit:
			Out.CastTarget.Actor = Target.Actor.Get();
			break;
		case EVeyraBotTargeting::Point:
			Out.CastTarget.bHasLocation = true;
			Out.CastTarget.Location = AimAt(View.Self, Target, Slot.Profile, Aim);
			break;
		case EVeyraBotTargeting::Self:
			break;
		}
		return Out;
	}

	/**
	 * A jungler's move once no fight, Well or shopping calls it (ADR-014 §7; League's jungler): gank a
	 * hurt enemy near and clear of an enemy tower; else clear its side's nearest standing camp, keeping
	 * at the creature it chose; else walk to the camp back soonest and wait there.
	 */
	FVeyraBotIntent DecideJungle(const FVeyraBotView& View, const FVeyraBotsTuning& Tuning, FVeyraBotMemory& Memory)
	{
		const FVeyraBotUnit* Prey = nullptr;
		for (const FVeyraBotUnit& Enemy : View.GankTargets)
		{
			if (Enemy.HealthFraction() < Tuning.Jungle.GankHealthFraction && !IsCoveredByTower(View, Enemy, Tuning.Senses.TowerMargin)
				&& (!Prey || EdgeDistance(View.Self, Enemy) < EdgeDistance(View.Self, *Prey)))
			{
				Prey = &Enemy;
			}
		}
		if (Prey)
		{
			return MoveTo(EVeyraBotAction::Move, Prey->Location, TEXT("ganking"));
		}

		const FVeyraBotCamp* ClosestCamp = nullptr;
		const FVeyraBotCamp* Soonest = nullptr;
		for (const FVeyraBotCamp& Camp : View.Camps)
		{
			if (Camp.Standing > 0
				&& (!ClosestCamp || FVector::Dist2D(View.Self.Location, Camp.Center) < FVector::Dist2D(View.Self.Location, ClosestCamp->Center)))
			{
				ClosestCamp = &Camp;
			}
			if (Camp.Standing == 0 && Camp.SpawnsAt > 0.0 && (!Soonest || Camp.SpawnsAt < Soonest->SpawnsAt))
			{
				Soonest = &Camp;
			}
		}
		// A camp it cannot see yet it walks to: nobody may attack what they cannot see (Vision Bible §1).
		if (ClosestCamp && ClosestCamp->Creatures.IsEmpty())
		{
			return MoveTo(EVeyraBotAction::Move, ClosestCamp->Center, TEXT("walking to its camp"));
		}
		if (ClosestCamp)
		{
			const FVeyraBotUnit* Chosen = ClosestCamp->Creatures.FindByPredicate([&Memory](const FVeyraBotUnit& Creature) { return Creature.Actor == Memory.FarmTarget; });
			if (!Chosen)
			{
				for (const FVeyraBotUnit& Creature : ClosestCamp->Creatures)
				{
					Chosen = !Chosen || Creature.Health < Chosen->Health ? &Creature : Chosen;
				}
			}
			Memory.FarmTarget = Chosen->Actor;
			return AttackOf(*Chosen, TEXT("clearing its camp"));
		}
		if (Soonest && FVector::Dist2D(View.Self.Location, Soonest->Center) > Tuning.Positioning.HoldTolerance)
		{
			return MoveTo(EVeyraBotAction::Move, Soonest->Center, TEXT("walking to its next camp"));
		}
		return Intent(EVeyraBotAction::Wait, TEXT("waiting for its camp"));
	}

	/** Slots in the order a bot tries them: its ultimate first, then Q, W and E. */
	TArray<const FVeyraBotSlot*> CastOrder(const FVeyraBotView& View)
	{
		TArray<const FVeyraBotSlot*> Order;
		for (const FVeyraBotSlot& Slot : View.Slots)
		{
			Order.Add(&Slot);
		}
		Order.StableSort([](const FVeyraBotSlot& A, const FVeyraBotSlot& B) { return IsUltimate(A.Slot) && !IsUltimate(B.Slot); });
		return Order;
	}
}

TOptional<FVeyraContentId> NextPurchase(const FVeyraItemsTuning& Items, TConstArrayView<FVeyraContentId> Build,
	TConstArrayView<FVeyraInventorySlot> Slots, TConstArrayView<FVeyraPendingPurchase> Queue, double Gold)
{
	for (const FVeyraContentId& Item : Build)
	{
		if (Holds(Slots, Queue, Item))
		{
			continue;
		}
		const FVeyraPurchaseQuote Quote = VeyraInventory::Quote(Items, Slots, Queue, Item);
		if (NeverBuyable(Quote.Refusal))
		{
			continue;
		}
		if (Quote.Refusal == EVeyraShopRefusal::None && Quote.Price <= Gold)
		{
			return Item;
		}
		TOptional<FVeyraContentId> Part;
		double PartPrice = 0.0;
		TMap<FVeyraContentId, int32> Unheld = HeldCounts(Slots, Queue);
		FindAffordablePart(Items, Item, Slots, Queue, Gold, Unheld, Part, PartPrice);
		// Nothing affordable toward it: save for it rather than skip ahead.
		return Part;
	}
	return {};
}

TOptional<EVeyraAbilitySlot> NextRank(TConstArrayView<EVeyraBotSkill> Priority, TFunctionRef<bool(EVeyraAbilitySlot)> CanRank)
{
	if (CanRank(EVeyraAbilitySlot::R))
	{
		return EVeyraAbilitySlot::R;
	}
	for (const EVeyraBotSkill Skill : Priority)
	{
		if (CanRank(SlotOf(Skill)))
		{
			return SlotOf(Skill);
		}
	}
	return {};
}

FVector AimAt(const FVeyraBotUnit& Caster, const FVeyraBotUnit& Target, const FVeyraBotAbilityProfile& Profile, EVeyraBotAim Aim)
{
	if (Aim != EVeyraBotAim::Lead)
	{
		return Target.Location;
	}
	double Seconds = Profile.LeadSeconds;
	if (Profile.ProjectileSpeed > 0.0)
	{
		Seconds += FVector::Dist2D(Caster.Location, Target.Location) / Profile.ProjectileSpeed;
	}
	return Target.Location + FVector(Target.Velocity.X, Target.Velocity.Y, 0.0) * Seconds;
}

double EdgeDistance(const FVeyraBotUnit& A, const FVeyraBotUnit& B)
{
	return FMath::Max(0.0, FVector::Dist2D(A.Location, B.Location) - A.Radius - B.Radius);
}

FVeyraBotIntent Decide(const FVeyraBotView& View, const FVeyraBotDifficultyTuning& Difficulty, const FVeyraBotsTuning& Tuning,
	FVeyraBotMemory& Memory, FRandomStream& Random)
{
	// Remember when each enemy Vanguard came into sight; forget those out of it.
	for (auto It = Memory.FirstSeen.CreateIterator(); It; ++It)
	{
		if (!View.EnemyVanguards.ContainsByPredicate([&It](const FVeyraBotUnit& Enemy) { return Enemy.Actor == It->Key; }))
		{
			It.RemoveCurrent();
		}
	}
	for (const FVeyraBotUnit& Enemy : View.EnemyVanguards)
	{
		Memory.FirstSeen.FindOrAdd(Enemy.Actor, View.Now);
	}

	if (!View.bAlive)
	{
		Memory.bRetreating = false;
		return Intent(EVeyraBotAction::Wait, TEXT("dead"));
	}
	if (View.bRecalling)
	{
		return Intent(EVeyraBotAction::Wait, TEXT("recalling"));
	}

	// Hurt: retreat, recall once safe, and heal at the fountain before going back (ADR-013 §8.2).
	const double Health = View.Self.HealthFraction();
	if (Health < Difficulty.RetreatHealthFraction)
	{
		Memory.bRetreating = true;
	}
	if (Memory.bRetreating)
	{
		if (View.bAtFountain)
		{
			if (Health < Tuning.Positioning.LeaveFountainHealthFraction)
			{
				return Intent(EVeyraBotAction::Wait, TEXT("healing at the fountain"));
			}
			Memory.bRetreating = false;
		}
		else
		{
			const FVeyraBotUnit* Threat = Nearest(View.Self, View.EnemyVanguards);
			if (!Threat || EdgeDistance(View.Self, *Threat) > Tuning.Senses.SafeRadius)
			{
				return Intent(EVeyraBotAction::Recall, TEXT("hurt and safe: recalling"));
			}
			for (const FVeyraBotSlot* Slot : CastOrder(View))
			{
				if (Slot->Use == EVeyraBotAbilityUse::Escape && Slot->bReady && EdgeDistance(View.Self, *Threat) <= Slot->Profile.Reach
					&& Random.FRand() < Difficulty.CastChance)
				{
					// Cast at the threat, to be carried away from it; no attack follows.
					FVeyraBotIntent Escape = CastOf(View, *Slot, *Threat, EVeyraBotAim::AtTarget, TEXT("escaping"));
					if (Slot->Profile.Targeting == EVeyraBotTargeting::Point && !Slot->Profile.bAwayFromPoint)
					{
						// A dash toward its point, as a Blink is: aimed home, away from the threat.
						const FVector Homeward = (View.Home - View.Self.Location).GetSafeNormal2D();
						Escape.CastTarget.Location = View.Self.Location + Homeward * Slot->Profile.Reach;
					}
					Escape.Target = nullptr;
					return Escape;
				}
				if (Slot->Use == EVeyraBotAbilityUse::Defend && Slot->bReady && Slot->Profile.Targeting == EVeyraBotTargeting::Self
					&& Random.FRand() < Difficulty.CastChance)
				{
					// A heal or guard on itself helps it get away, as League's bots Heal as they run.
					return CastOf(View, *Slot, View.Self, EVeyraBotAim::AtTarget, TEXT("hurt: defending itself"));
				}
			}
			return MoveTo(EVeyraBotAction::Retreat, View.Home, TEXT("hurt: retreating"));
		}
	}

	// A tower shooting it: step back out of range.
	if (View.EnemyStructure.IsSet() && View.EnemyStructure->bTargetsBot)
	{
		return MoveTo(EVeyraBotAction::Retreat, View.LaneHold, TEXT("under tower fire: backing off"));
	}

	// A creature or Flux Well in reach that a Secure spell would finish: take it before anyone else
	// can, the largest first, as League's junglers Smite (ADR-015 §8).
	for (const FVeyraBotSlot* Slot : CastOrder(View))
	{
		if (Slot->Use != EVeyraBotAbilityUse::Secure || !Slot->bReady)
		{
			continue;
		}
		const FVeyraBotUnit* Finish = nullptr;
		const auto Consider = [&View, Slot, &Finish](const FVeyraBotUnit& Unit) {
			const double Dealt = Slot->Profile.Damage * (Slot->Profile.bTrueDamage ? 1.0 : Unit.DamageTaken);
			if (Unit.Health > 0.0 && Unit.Health <= Dealt && EdgeDistance(View.Self, Unit) <= Slot->Profile.Reach && (!Finish || Unit.MaxHealth > Finish->MaxHealth))
			{
				Finish = &Unit;
			}
		};
		for (const FVeyraBotUnit& Well : View.Wells)
		{
			Consider(Well);
		}
		for (const FVeyraBotCamp& Camp : View.Camps)
		{
			for (const FVeyraBotUnit& Creature : Camp.Creatures)
			{
				Consider(Creature);
			}
		}
		if (Finish)
		{
			return CastOf(View, *Slot, *Finish, EVeyraBotAim::AtTarget, TEXT("securing"));
		}
	}

	// Fight the weakest enemy Vanguard it has watched long enough, if the trade favours it, no enemy
	// tower covers the foe, and the enemy wave would not turn on it (ADR-013 §8.4, §8.5; Battleground
	// Bible §19).
	const FVeyraBotUnit* Foe = nullptr;
	for (const FVeyraBotUnit& Enemy : View.EnemyVanguards)
	{
		if (Enemy.Defenders > Difficulty.FluxbornTolerance)
		{
			continue;
		}
		const double* SeenAt = Memory.FirstSeen.Find(Enemy.Actor);
		const bool bWatched = SeenAt && View.Now - *SeenAt >= Difficulty.ReactionSeconds;
		const bool bFavoured = Health >= Enemy.HealthFraction() + Difficulty.FightHealthMargin;
		if (bWatched && bFavoured && !IsCoveredByTower(View, Enemy, Tuning.Senses.TowerMargin) && (!Foe || Enemy.HealthFraction() < Foe->HealthFraction()))
		{
			Foe = &Enemy;
		}
	}
	if (Foe)
	{
		const double Distance = EdgeDistance(View.Self, *Foe);
		for (const FVeyraBotSlot* Slot : CastOrder(View))
		{
			if (!Slot->bReady)
			{
				continue;
			}
			bool bUseful = false;
			switch (Slot->Use)
			{
			case EVeyraBotAbilityUse::Damage:
			case EVeyraBotAbilityUse::Engage:
				// Only one that may target a Vanguard.
				bUseful = Distance <= Slot->Profile.Reach && (Slot->Profile.TargetKinds.IsEmpty() || Slot->Profile.TargetKinds.Contains(EVeyraUnitKind::Vanguard));
				break;
			case EVeyraBotAbilityUse::Empower:
			case EVeyraBotAbilityUse::Defend:
				bUseful = Distance <= View.AttackRange;
				break;
			case EVeyraBotAbilityUse::Escape:
			case EVeyraBotAbilityUse::Secure:
				break;
			}
			if (bUseful && Random.FRand() < Difficulty.CastChance)
			{
				return CastOf(View, *Slot, *Foe, Difficulty.Aim, TEXT("fighting: casting"));
			}
		}
		return AttackOf(*Foe, TEXT("fighting"));
	}

	// Gold to spend and a quiet lane: back to shop (ADR-013 §8.2).
	if (!View.bAtFountain && View.bPurchaseWaiting && View.Gold >= Difficulty.ShopRecallGold)
	{
		const FVeyraBotUnit* Threat = Nearest(View.Self, View.EnemyVanguards);
		if (!Threat || EdgeDistance(View.Self, *Threat) > Tuning.Senses.SafeRadius)
		{
			return Intent(EVeyraBotAction::Recall, TEXT("Gold to spend: recalling to shop"));
		}
	}

	// A warding seat wards the fog patches it passes that no ward of its side covers, with a charge
	// and no enemy Vanguard near, as League's jungler and support ward the bushes (ADR-016 §7).
	if (View.bWards && View.WardCharges > 0)
	{
		const FVeyraBotUnit* Near = Nearest(View.Self, View.EnemyVanguards);
		if (!Near || EdgeDistance(View.Self, *Near) > Tuning.Senses.SafeRadius)
		{
			for (const FVector& Spot : View.WardSpots)
			{
				const bool bCovered = View.AlliedWards.ContainsByPredicate(
					[&Spot, &Tuning](const FVector& Ward) { return FVector::Dist2D(Ward, Spot) <= Tuning.Warding.SpotSpacing; });
				if (!bCovered && FVector::Dist2D(View.Self.Location, Spot) <= Tuning.Warding.SpotReach)
				{
					return MoveTo(EVeyraBotAction::Ward, Spot, TEXT("warding"));
				}
			}
		}
	}

	// An open Flux Well within its reach and no enemy Vanguard near: take it (ADR-014 §7), as League's
	// bots take an objective when their lane allows.
	const FVeyraBotUnit* Well = Nearest(View.Self, View.Wells);
	const double WellReach = View.bJungle ? Tuning.Jungle.WellRange : Tuning.Positioning.WellRange;
	const FVeyraBotUnit* Threat = Nearest(View.Self, View.EnemyVanguards);
	if (Well && EdgeDistance(View.Self, *Well) <= WellReach && (!Threat || EdgeDistance(View.Self, *Threat) > Tuning.Senses.SafeRadius))
	{
		return AttackOf(*Well, TEXT("taking a Flux Well"));
	}

	if (View.bJungle)
	{
		return DecideJungle(View, Tuning, Memory);
	}

	// Last-hit a Fluxborn about to die: within a lead of one basic attack, which covers walking up
	// and winding up. Push with the weakest one near it otherwise. Never where an enemy tower would
	// shoot it for that, unless its wave holds the tower.
	const FVeyraBotUnit* Prey = nullptr;
	const FVeyraBotUnit* Weakest = nullptr;
	const FVeyraBotUnit* Committed = nullptr;
	const bool bWaveHoldsTower = View.EnemyStructure.IsSet() && View.EnemyStructure->bAlliesInRange;
	for (const FVeyraBotUnit& Fluxborn : View.EnemyFluxborn)
	{
		if (IsCoveredByTower(View, Fluxborn, 0.0) && !bWaveHoldsTower)
		{
			continue;
		}
		Committed = Fluxborn.Actor == Memory.FarmTarget ? &Fluxborn : Committed;
		const double Distance = EdgeDistance(View.Self, Fluxborn);
		if (Fluxborn.Health <= View.AttackDamage * Fluxborn.DamageTaken * Difficulty.LastHitLead && (!Prey || Distance < EdgeDistance(View.Self, *Prey)))
		{
			Prey = &Fluxborn;
		}
		if (Distance <= Tuning.Positioning.PushRange && (!Weakest || Fluxborn.Health < Weakest->Health))
		{
			Weakest = &Fluxborn;
		}
	}
	if (Prey && Random.FRand() < Difficulty.LastHitChance)
	{
		Memory.FarmTarget = Prey->Actor;
		return AttackOf(*Prey, TEXT("last-hitting"));
	}

	// Keep attacking the Fluxborn it chose: a new choice each decision would throw its windups away.
	if (Committed)
	{
		return AttackOf(*Committed, TEXT("keeping at its Fluxborn"));
	}

	// Siege with its wave: a structure it can damage while the tower shoots the wave (ADR-013 §8.4).
	if (View.EnemyStructure.IsSet() && View.EnemyStructure->bVulnerable && View.EnemyStructure->bAlliesInRange)
	{
		return AttackOf(View.EnemyStructure->Unit, TEXT("sieging with its wave"));
	}

	// Shove the wave, as League's bots do, so lanes push and structures come under siege.
	if (Weakest && Random.FRand() < Difficulty.PushChance)
	{
		Memory.FarmTarget = Weakest->Actor;
		return AttackOf(*Weakest, TEXT("pushing the wave"));
	}

	if (FVector::Dist2D(View.Self.Location, View.LaneHold) > Tuning.Positioning.HoldTolerance)
	{
		return MoveTo(EVeyraBotAction::Move, View.LaneHold, TEXT("taking its place in lane"));
	}
	return Intent(EVeyraBotAction::Wait, TEXT("holding its place"));
}
}
