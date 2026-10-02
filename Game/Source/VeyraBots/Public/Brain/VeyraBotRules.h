// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Brain/VeyraBotView.h"
#include "Containers/ArrayView.h"
#include "Math/RandomStream.h"
#include "Templates/Function.h"

struct FVeyraInventorySlot;
struct FVeyraItemsTuning;
struct FVeyraPendingPurchase;

/**
 * How a bot decides (ADR-013 §4, §8), as pure functions of what it knows: no world, so each rule is
 * tested on its own. Chances are drawn from the stream the caller seeds, so a match replays alike.
 */
namespace VeyraBotRules
{
	/**
	 * What to buy next toward Build (ADR-013 §8.3): the first item it does not hold or await, when
	 * Gold affords it; otherwise the dearest affordable part of that item's recipe, at any depth.
	 * Nothing when it can afford neither: it saves for it. An item it may never hold (unique, a second
	 * pair of Boots, no free slot, a Mythical other than its own, a second item of a quest line) is passed
	 * over, and so is a Quest Item once what it became is held. A recipe waiting for a quest's evolution
	 * buys its other parts, and then the bot moves on. Mythical is the bot's Mythical this match, invalid
	 * until it buys one.
	 */
	VEYRABOTS_API TOptional<FVeyraContentId> NextPurchase(const FVeyraItemsTuning& Items, TConstArrayView<FVeyraContentId> Build,
		TConstArrayView<FVeyraInventorySlot> Slots, TConstArrayView<FVeyraPendingPurchase> Queue, const FVeyraContentId& Mythical, double Gold);

	/**
	 * The consumable to buy next (ADR-056 §1): Consumables' item while the bot holds and awaits fewer than Carried, the
	 * match is younger than its last buying time, and the shop would sell one that Gold affords; nothing otherwise. A new
	 * stack never takes the last free slot, which a recipe's part may need. The caller asks only once NextPurchase has
	 * nothing, so the build is never starved.
	 */
	VEYRABOTS_API TOptional<FVeyraContentId> NextConsumable(const FVeyraItemsTuning& Items, const FVeyraBotConsumablesTuning& Consumables, int32 Carried,
		TConstArrayView<FVeyraInventorySlot> Slots, TConstArrayView<FVeyraPendingPurchase> Queue, const FVeyraContentId& Mythical, double Gold,
		double MatchSeconds);

	/**
	 * The inventory slot, from 0, to drink from now (ADR-056 §1): the one holding the consumable, while the bot is alive,
	 * below its drinking line, away from its fountain and not recalling; nothing otherwise. The shop refuses a drink while
	 * one still restores.
	 */
	VEYRABOTS_API TOptional<int32> NextDrink(const FVeyraBotView& View, const FVeyraBotConsumablesTuning& Consumables);

	/**
	 * Whether a dead bot buys back now (ADR-056 §3): its difficulty buys back, its base is under threat, it would
	 * otherwise wait at least the least wait, the economy allows it, and its Gold covers the cost and its reserve.
	 */
	VEYRABOTS_API bool ShouldBuyBack(const FVeyraBotView& View, const FVeyraBotDifficultyTuning& Difficulty, const FVeyraBotBuybackTuning& Buyback);

	/**
	 * The lane a side's laners push together late in a match (ADR-056 §4): the one where the most enemy structures have
	 * fallen, by Fallen, the first of Order on a tie. Mid without an order.
	 */
	VEYRABOTS_API EVeyraLane GroupLane(const TMap<EVeyraLane, int32>& Fallen, TConstArrayView<EVeyraLane> Order);

	/**
	 * Which slot to rank next: R whenever it may, then the first of Priority that may. CanRank says
	 * whether a slot may take a rank now. Nothing when none may.
	 */
	VEYRABOTS_API TOptional<EVeyraAbilitySlot> NextRank(TConstArrayView<EVeyraBotSkill> Priority, TFunctionRef<bool(EVeyraAbilitySlot)> CanRank);

	/**
	 * Where to aim at Target: where it is, or, leading it, where it will be when the cast lands,
	 * after the profile's delays and a projectile's travel from Caster.
	 */
	VEYRABOTS_API FVector AimAt(const FVeyraBotUnit& Caster, const FVeyraBotUnit& Target, const FVeyraBotAbilityProfile& Profile, EVeyraBotAim Aim);

	/** The distance between two units' edges, on the ground. */
	VEYRABOTS_API double EdgeDistance(const FVeyraBotUnit& A, const FVeyraBotUnit& B);

	/**
	 * The decision, in priority order (ADR-013 §4): stay dead, recalling or healing; retreat and recall when hurt; step
	 * out of a tower shooting it; secure a creature or Well; guard a chased ally; fight the weakest enemy Vanguard it has
	 * watched long enough when the trade favours it; answer a threat to its base (ADR-056 §2); recall to shop; ward; take
	 * a Flux Well; a jungler's own steps; last-hit, keep at its Fluxborn, siege with its wave and push; else hold its place
	 * in lane, or by its threatened base structure.
	 */
	VEYRABOTS_API FVeyraBotIntent Decide(const FVeyraBotView& View, const FVeyraBotDifficultyTuning& Difficulty, const FVeyraBotsTuning& Tuning,
		FVeyraBotMemory& Memory, FRandomStream& Random);
}
