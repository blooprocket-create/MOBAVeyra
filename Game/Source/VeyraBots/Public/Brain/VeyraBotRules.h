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
	 * pair of Boots, no free slot, a Mythical other than its own) is passed over. Mythical is the bot's
	 * Mythical this match, invalid until it buys one.
	 */
	VEYRABOTS_API TOptional<FVeyraContentId> NextPurchase(const FVeyraItemsTuning& Items, TConstArrayView<FVeyraContentId> Build,
		TConstArrayView<FVeyraInventorySlot> Slots, TConstArrayView<FVeyraPendingPurchase> Queue, const FVeyraContentId& Mythical, double Gold);

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
	 * The decision, in League's priority order: stay dead, recalling or healing; retreat and recall
	 * when hurt; step out of a tower shooting it; fight the weakest enemy Vanguard it has watched long
	 * enough when the trade favours it and no enemy tower covers it; last-hit a Fluxborn; siege with
	 * its wave; else hold its place in lane.
	 */
	VEYRABOTS_API FVeyraBotIntent Decide(const FVeyraBotView& View, const FVeyraBotDifficultyTuning& Difficulty, const FVeyraBotsTuning& Tuning,
		FVeyraBotMemory& Memory, FRandomStream& Random);
}
