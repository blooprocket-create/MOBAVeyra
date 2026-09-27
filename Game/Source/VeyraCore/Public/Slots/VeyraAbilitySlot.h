// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "UObject/ObjectMacros.h"

#include "VeyraAbilitySlot.generated.h"

/**
 * A Vanguard's ability slots (Settings Bible §1.2: Q/W/E/R). Abilities, Progression (ranks) and
 * Match (intents) all name them, so the vocabulary lives here (ADR-008 §1). Basic attacks, Flux
 * Spells and item actives get theirs later.
 */
UENUM()
enum class EVeyraAbilitySlot : uint8
{
	Q,
	W,
	E,
	/** The ultimate. */
	R,
};

namespace VeyraAbilitySlots
{
	/** Every slot, in order. */
	inline constexpr EVeyraAbilitySlot All[] = { EVeyraAbilitySlot::Q, EVeyraAbilitySlot::W, EVeyraAbilitySlot::E, EVeyraAbilitySlot::R };

	/** Whether Slot holds the ultimate, whose ranks open at set levels (Economy & Progression §1). */
	inline constexpr bool IsUltimate(EVeyraAbilitySlot Slot)
	{
		return Slot == EVeyraAbilitySlot::R;
	}
}
