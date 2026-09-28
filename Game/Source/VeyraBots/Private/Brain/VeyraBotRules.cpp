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

	/** The dearest part of Item's recipe, at any depth, that Gold buys now and is not already held. */
	void FindAffordablePart(const FVeyraItemsTuning& Items, const FVeyraContentId& Item, TConstArrayView<FVeyraInventorySlot> Slots,
		TConstArrayView<FVeyraPendingPurchase> Queue, double Gold, TOptional<FVeyraContentId>& Best, double& BestPrice)
	{
		const FVeyraItemDefinition* Definition = Items.Items.Find(Item);
		if (!Definition)
		{
			return;
		}
		for (const FVeyraContentId& Part : Definition->Components)
		{
			if (Holds(Slots, Queue, Part))
			{
				continue;
			}
			const FVeyraPurchaseQuote Quote = VeyraInventory::Quote(Items, Slots, Queue, Part);
			if (Quote.Refusal == EVeyraShopRefusal::None && Quote.Price <= Gold && Quote.Price > BestPrice)
			{
				Best = Part;
				BestPrice = Quote.Price;
			}
			FindAffordablePart(Items, Part, Slots, Queue, Gold, Best, BestPrice);
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
		FindAffordablePart(Items, Item, Slots, Queue, Gold, Part, PartPrice);
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
					Escape.Target = nullptr;
					return Escape;
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

	// Fight the weakest enemy Vanguard it has watched long enough, if the trade favours it and no
	// enemy tower covers either of them (ADR-013 §8.4, §8.5).
	const FVeyraBotUnit* Foe = nullptr;
	for (const FVeyraBotUnit& Enemy : View.EnemyVanguards)
	{
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
				bUseful = Distance <= Slot->Profile.Reach;
				break;
			case EVeyraBotAbilityUse::Empower:
			case EVeyraBotAbilityUse::Defend:
				bUseful = Distance <= View.AttackRange;
				break;
			case EVeyraBotAbilityUse::Escape:
				break;
			}
			if (bUseful && Random.FRand() < Difficulty.CastChance)
			{
				return CastOf(View, *Slot, *Foe, Difficulty.Aim, TEXT("fighting: casting"));
			}
		}
		return AttackOf(*Foe, TEXT("fighting"));
	}

	// Last-hit a Fluxborn one basic attack kills, unless an enemy tower would shoot it for that.
	const FVeyraBotUnit* Prey = nullptr;
	for (const FVeyraBotUnit& Fluxborn : View.EnemyFluxborn)
	{
		const bool bSafe = !IsCoveredByTower(View, Fluxborn, 0.0) || (View.EnemyStructure.IsSet() && View.EnemyStructure->bAlliesInRange);
		if (Fluxborn.Health <= View.AttackDamage * Fluxborn.DamageTaken && bSafe && (!Prey || EdgeDistance(View.Self, Fluxborn) < EdgeDistance(View.Self, *Prey)))
		{
			Prey = &Fluxborn;
		}
	}
	if (Prey && Random.FRand() < Difficulty.LastHitChance)
	{
		return AttackOf(*Prey, TEXT("last-hitting"));
	}

	// Siege with its wave: a structure it can damage while the tower shoots the wave (ADR-013 §8.4).
	if (View.EnemyStructure.IsSet() && View.EnemyStructure->bVulnerable && View.EnemyStructure->bAlliesInRange)
	{
		return AttackOf(View.EnemyStructure->Unit, TEXT("sieging with its wave"));
	}

	if (FVector::Dist2D(View.Self.Location, View.LaneHold) > Tuning.Positioning.HoldTolerance)
	{
		return MoveTo(EVeyraBotAction::Move, View.LaneHold, TEXT("taking its place in lane"));
	}
	return Intent(EVeyraBotAction::Wait, TEXT("holding its place"));
}
}
