// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Array.h"
#include "Content/VeyraContentId.h"
#include "Misc/Optional.h"
#include "Slots/VeyraAbilitySlot.h"
#include "Statuses/VeyraStatusTypes.h"

class AActor;
class AVeyraPlayerState;

/** A unit's bars, as the overhead display and the HUD show them. */
struct FVeyraHudVitals
{
	double Health = 0.0;
	double MaxHealth = 0.0;

	/** Every shield's remaining amount, together (Combat Bible §7). */
	double Shield = 0.0;

	double Resource = 0.0;
	double MaxResource = 0.0;
};

/** A status on a unit, with the time it has left. */
struct FVeyraHudStatus
{
	FVeyraContentId Id;
	EVeyraStatusKind Kind = EVeyraStatusKind::Stun;
	double RemainingSeconds = 0.0;
};

/** One ability slot on the player's panel. */
struct FVeyraHudSlot
{
	EVeyraAbilitySlot Slot = EVeyraAbilitySlot::Q;

	/** Invalid when the slot holds no ability. */
	FVeyraContentId Ability;

	int32 Rank = 0;
	int32 MaxRank = 0;

	/** Whether a skill point may go into it now (Economy & Progression Bible §1, §9). */
	bool bCanRankUp = false;

	/** Seconds until it is ready; 0 when it is. */
	double CooldownSeconds = 0.0;
};

/** The player's own panel. */
struct FVeyraHudPlayer
{
	FVeyraContentId Vanguard;
	int32 Level = 0;

	/** XP toward the next level, and what that level needs; both 0 at the cap. */
	int32 Experience = 0;
	int32 ExperienceToNextLevel = 0;

	int32 UnspentSkillPoints = 0;
	FVeyraHudVitals Vitals;

	/** Q, W, E and R, in order. */
	TArray<FVeyraHudSlot> Slots;
};

/**
 * What the HUD shows, read from replicated state (ARCHITECTURE.md §3: the HUD displays what
 * gameplay supplies and calculates none of it). Plain functions, so tests check them without a
 * canvas.
 */
namespace VeyraHud
{
	/** Unit's bars, from its Ability System Component and shields; nothing when it has neither Health nor an Ability System Component. */
	VEYRAUI_API TOptional<FVeyraHudVitals> VitalsOf(const AActor& Unit);

	/** Unit's statuses in the order they were applied, each with its time left at ServerNow, in server gameplay time. */
	VEYRAUI_API TArray<FVeyraHudStatus> StatusesOf(const AActor& Unit, double ServerNow);

	/** Participant's panel at ServerNow, in server gameplay time. */
	VEYRAUI_API FVeyraHudPlayer DescribePlayer(const AVeyraPlayerState& Participant, double ServerNow);
}
