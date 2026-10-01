// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Containers/Array.h"

class AActor;
struct FVeyraDeathEvent;
struct FVeyraInventorySlot;
struct FVeyraItemsTuning;
enum class EVeyraQuestObjective : uint8;

/** Quest Items' rules (Item Bible §2.5, §10; ADR-025 §3), as pure functions of the catalog. */
namespace VeyraQuests
{
	/**
	 * The participant whose Vanguard landed Death's killing blow on an enemy lane Fluxborn (Item Bible
	 * §10): only such a last hit counts, never wildlife, a structure or a unit of another kind, nor a
	 * blow another Fluxborn landed. Null for any other death.
	 */
	VEYRAITEMS_API AActor* LaneFluxbornLastHitter(const FVeyraDeathEvent& Death);

	/**
	 * Advances by one each quest in Slots that Objective advances. One that reaches its threshold
	 * evolves in place, at no cost: the slot keeps the Gold its base form cost, so it resells at that
	 * price, and has given benefit, so undo cannot take it back. Returns the items evolved into.
	 */
	VEYRAITEMS_API TArray<FVeyraContentId> Advance(const FVeyraItemsTuning& Tuning, TArray<FVeyraInventorySlot>& Slots, EVeyraQuestObjective Objective);
}
