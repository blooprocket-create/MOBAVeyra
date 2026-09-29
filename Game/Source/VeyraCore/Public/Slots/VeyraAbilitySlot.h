// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "UObject/ObjectMacros.h"

#include "VeyraAbilitySlot.generated.h"

/**
 * A Vanguard's ability slots (Settings Bible §1.2: Q/W/E/R), the six item slots, whose keys use the
 * item in that inventory slot (author ruling 2026-09-28; ADR-012 §1), and the two Flux Spell slots
 * (Battleground Bible §14; ADR-015 §1). Abilities, Progression (ranks) and Match (intents) all name
 * them, so the vocabulary lives here (ADR-008 §1). Basic attacks get theirs later.
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
	/** The two Flux Spell slots: no ranks, fixed cooldowns, locked until permanent Team Flux opens them (ADR-015). */
	Spell1,
	Spell2,
};

namespace VeyraAbilitySlots
{
	/** Every kit slot, in order: the ones that take ranks. */
	inline constexpr EVeyraAbilitySlot All[] = { EVeyraAbilitySlot::Q, EVeyraAbilitySlot::W, EVeyraAbilitySlot::E, EVeyraAbilitySlot::R };

	/** Every item slot, in inventory order. */
	inline constexpr EVeyraAbilitySlot Items[] = { EVeyraAbilitySlot::Item1, EVeyraAbilitySlot::Item2, EVeyraAbilitySlot::Item3,
		EVeyraAbilitySlot::Item4, EVeyraAbilitySlot::Item5, EVeyraAbilitySlot::Item6 };

	/** The Flux Spell slots, in unlock order: the first opens at the lower permanent-Flux threshold. */
	inline constexpr EVeyraAbilitySlot Spells[] = { EVeyraAbilitySlot::Spell1, EVeyraAbilitySlot::Spell2 };

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

	/** Whether Slot is a Flux Spell slot: no ranks, and a fixed cooldown no Haste shortens (Combat Bible §21). */
	inline constexpr bool IsSpellSlot(EVeyraAbilitySlot Slot)
	{
		return Slot == EVeyraAbilitySlot::Spell1 || Slot == EVeyraAbilitySlot::Spell2;
	}

	/** Spell slot index, from 0, of a Flux Spell slot; INDEX_NONE for any other. */
	inline constexpr int32 SpellIndexOf(EVeyraAbilitySlot Slot)
	{
		return IsSpellSlot(Slot) ? static_cast<int32>(Slot) - static_cast<int32>(EVeyraAbilitySlot::Spell1) : INDEX_NONE;
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
