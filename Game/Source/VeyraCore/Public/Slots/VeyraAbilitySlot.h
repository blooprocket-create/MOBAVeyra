// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "UObject/ObjectMacros.h"

#include "VeyraAbilitySlot.generated.h"

/**
 * A Vanguard's ability slots (Settings Bible §1.2: Q/W/E/R), and the six item slots, whose keys use
 * the item in that inventory slot (author ruling 2026-09-28; ADR-012 §1). Abilities, Progression
 * (ranks) and Match (intents) all name them, so the vocabulary lives here (ADR-008 §1). Basic attacks
 * and Flux Spells get theirs later.
 */
UENUM()
enum class EVeyraAbilitySlot : uint8
{
	Q,
	W,
	E,
	/** The ultimate. */
	R,
	/** Inventory slots 1–6: an item's Active is cast from the slot the item sits in. */
	Item1,
	Item2,
	Item3,
	Item4,
	Item5,
	Item6,
};

namespace VeyraAbilitySlots
{
	/** Every kit slot, in order: the ones that take ranks. */
	inline constexpr EVeyraAbilitySlot All[] = { EVeyraAbilitySlot::Q, EVeyraAbilitySlot::W, EVeyraAbilitySlot::E, EVeyraAbilitySlot::R };

	/** Every item slot, in inventory order. */
	inline constexpr EVeyraAbilitySlot Items[] = { EVeyraAbilitySlot::Item1, EVeyraAbilitySlot::Item2, EVeyraAbilitySlot::Item3,
		EVeyraAbilitySlot::Item4, EVeyraAbilitySlot::Item5, EVeyraAbilitySlot::Item6 };

	/** Whether Slot holds the ultimate, whose ranks open at set levels (Economy & Progression §1). */
	inline constexpr bool IsUltimate(EVeyraAbilitySlot Slot)
	{
		return Slot == EVeyraAbilitySlot::R;
	}

	/** Whether Slot is an item slot: no ranks, and its cooldown is the item's (Combat Bible §21). */
	inline constexpr bool IsItemSlot(EVeyraAbilitySlot Slot)
	{
		return Slot >= EVeyraAbilitySlot::Item1 && Slot <= EVeyraAbilitySlot::Item6;
	}

	/** The item slot for inventory slot Index, from 0; none past the last. */
	inline constexpr bool ItemSlotAt(int32 Index, EVeyraAbilitySlot& OutSlot)
	{
		if (Index < 0 || Index >= static_cast<int32>(UE_ARRAY_COUNT(Items)))
		{
			return false;
		}
		OutSlot = Items[Index];
		return true;
	}

	/** Inventory slot index, from 0, of an item slot; INDEX_NONE for a kit slot. */
	inline constexpr int32 ItemIndexOf(EVeyraAbilitySlot Slot)
	{
		return IsItemSlot(Slot) ? static_cast<int32>(Slot) - static_cast<int32>(EVeyraAbilitySlot::Item1) : INDEX_NONE;
	}
}
