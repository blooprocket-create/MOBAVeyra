// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Slots/VeyraAbilitySlot.h"

/** How an ability's key starts its cast (Settings Bible §1.2; ADR-041 §1). */
enum class EVeyraCastMode : uint8
{
	/** The press casts toward the cursor. */
	Quick,
	/** The press shows the indicator; the release casts toward the cursor. */
	QuickWithIndicator,
	/** The press shows the indicator; a click casts. */
	Normal,
};

/** The ability whose indicator the player sees, waiting to be cast or only previewed. */
struct FVeyraCastIndicator
{
	EVeyraAbilitySlot Slot = EVeyraAbilitySlot::Q;
	EVeyraCastMode Mode = EVeyraCastMode::Quick;
	/** Shown by Show Cast Range (Settings Bible §1.7): it never casts. */
	bool bPreviewOnly = false;
	/** The ability the slot held as it showed; invalid for a slot that holds none, such as the vision tool's. */
	FVeyraContentId Ability;
};

/** What a slot holds now, as its owner's client sees it, for a waiting cast to check it can still be cast. */
struct FVeyraSlotNow
{
	bool bCasterAlive = false;
	/** Invalid when the slot holds no ability. */
	FVeyraContentId Ability;
	/** A Flux Spell slot not unlocked yet. */
	bool bLocked = false;
	double CooldownSeconds = 0.0;
};

/** What the controller does after an input. */
enum class EVeyraCastStep : uint8
{
	Nothing,
	/** Cast the slot toward the cursor now. */
	CastNow,
	/** Show the slot's indicator. */
	Show,
	/** Hide the indicator. */
	Hide,
};

struct FVeyraCastOutcome
{
	EVeyraCastStep Step = EVeyraCastStep::Nothing;
	EVeyraAbilitySlot Slot = EVeyraAbilitySlot::Q;
};

/**
 * The player's cast input, apart from keys and cursors (ADR-041 §1): which indicator shows, and when a key,
 * its release, a click or a cancel casts. It decides nothing about the cast itself, which the server judges.
 */
class VEYRAMATCH_API FVeyraCastInput
{
public:
	/** Slot's key went down, cast in Mode; bPreview while the Show Cast Range modifier is held. Ability is what Slot holds. */
	FVeyraCastOutcome Press(EVeyraAbilitySlot Slot, EVeyraCastMode Mode, bool bPreview, const FVeyraContentId& Ability = FVeyraContentId());

	/** Slot's key came up: a Quick Cast with Indicator casts. */
	FVeyraCastOutcome Release(EVeyraAbilitySlot Slot);

	/** The select click: a waiting cast casts. */
	FVeyraCastOutcome Confirm();

	/** Escape or a right click: whatever shows is hidden. */
	FVeyraCastOutcome Cancel();

	/** The Show Cast Range modifier was let go: a preview is hidden. */
	FVeyraCastOutcome EndPreview();

	/**
	 * Checks a waiting cast against what its slot holds Now: it is hidden once the ability can no longer be cast
	 * (ADR-041 §1), because its caster died, its slot was locked, holds another ability or none, or it cools down.
	 * Crowd control does not hide it; the server refuses a cast made meanwhile. A preview stays.
	 */
	FVeyraCastOutcome Recheck(const FVeyraSlotNow& Now);

	const TOptional<FVeyraCastIndicator>& GetIndicator() const { return Indicator; }

private:
	FVeyraCastOutcome Take(EVeyraCastStep Step);

	TOptional<FVeyraCastIndicator> Indicator;
};

namespace VeyraCastModes
{
	/** The mode's name as the settings registry's options spell it, and back; unset for anything else. */
	VEYRAMATCH_API FString NameOf(EVeyraCastMode Mode);
	VEYRAMATCH_API TOptional<EVeyraCastMode> Parse(const FString& Name);
}
