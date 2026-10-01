// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Input/VeyraCastInput.h"
#include "VeyraMatchTypes.h"

class FVeyraSettingsStore;

/** The player's controls as they set them (Settings Bible §1.2; ADR-040 §5). */
struct FVeyraControlPreferences
{
	/** Each casting-mode setting's mode; one missing casts Quick. */
	TMap<FVeyraContentId, EVeyraCastMode> CastModes;

	/** How Slot's key casts. */
	VEYRAMATCH_API EVeyraCastMode CastModeOf(EVeyraAbilitySlot Slot) const;

	/** The kit slots whose ability names its caster when no allied Vanguard is under the cursor (Settings Bible §1.5). */
	TSet<EVeyraAbilitySlot> SmartSelfCast;

	/** Target Vanguards Only's key switches it with each press, instead of holding it while held (Settings Bible §1.4). */
	bool bTargetVanguardsToggles = false;

	/** Which enemy an attack-move takes first (Settings Bible §1.3). */
	EVeyraAttackMoveTarget AttackMoveTarget = EVeyraAttackMoveTarget::ClosestToVanguard;
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

	/** The Smart Self-Cast setting of a kit slot; null for any other slot. */
	VEYRAMATCH_API const FVeyraContentId* SmartSelfCast(EVeyraAbilitySlot Slot);

	/** Target Vanguards Only's Hold or Toggle. */
	VEYRAMATCH_API const FVeyraContentId& TargetVanguardsMode();

	/** Attack-move's target preference: ClosestToVanguard or ClosestToCursor. */
	VEYRAMATCH_API const FVeyraContentId& AttackMoveTarget();

	/** The player's controls in Store; every slot Quick without a store (a server, a test, a game without settings). */
	VEYRAMATCH_API FVeyraControlPreferences Resolve(const FVeyraSettingsStore* Store);
}
