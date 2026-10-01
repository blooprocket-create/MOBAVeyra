// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Input/VeyraCastInput.h"

class FVeyraSettingsStore;

/** The player's controls as they set them (Settings Bible §1.2; ADR-040 §5). */
struct FVeyraControlPreferences
{
	/** Each casting-mode setting's mode; one missing casts Quick. */
	TMap<FVeyraContentId, EVeyraCastMode> CastModes;

	/** How Slot's key casts. */
	VEYRAMATCH_API EVeyraCastMode CastModeOf(EVeyraAbilitySlot Slot) const;
};

/** The player's control settings, apart from the engine. */
namespace VeyraControlPreferences
{
	/**
	 * The casting-mode setting Slot follows: one for each kit slot and Flux Spell, and one for every
	 * item slot and the vision tool, whose contents move between slots.
	 */
	VEYRAMATCH_API const FVeyraContentId& CastMode(EVeyraAbilitySlot Slot);

	/** Every casting-mode setting, as the registry names them. */
	VEYRAMATCH_API TArray<FVeyraContentId> CastModeSettings();

	/** The player's controls in Store; every slot Quick without a store (a server, a test, a game without settings). */
	VEYRAMATCH_API FVeyraControlPreferences Resolve(const FVeyraSettingsStore* Store);
}
