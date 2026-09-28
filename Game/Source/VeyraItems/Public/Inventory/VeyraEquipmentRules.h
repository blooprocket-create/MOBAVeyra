// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Content/VeyraContentId.h"
#include "Stats/VeyraEquipmentStats.h"

struct FVeyraInventorySlot;
struct FVeyraItemsTuning;

/** What a participant's items add to its stats, as a pure function of the catalog (ADR-012 §6). */
namespace VeyraEquipment
{
	/**
	 * Everything Slots' items add: their flat stats, each stack counted; their bonus Attack Speed, a
	 * fraction of BaseAttackSpeed so it adds to level growth rather than compounding; and their
	 * Attunements (Item Bible §8–§9), each counted once: Weight of War's Physical Power from the items'
	 * bonus Health, Overcharge's share of Magic Power, and Spool Up's and Overcycle's per stack held,
	 * up to their caps, as Stacks counts them by Attunement.
	 */
	VEYRAITEMS_API FVeyraEquipmentStats StatsFor(const FVeyraItemsTuning& Tuning, TConstArrayView<FVeyraInventorySlot> Slots, double BaseAttackSpeed,
		const TMap<FVeyraContentId, int32>& Stacks);
}
