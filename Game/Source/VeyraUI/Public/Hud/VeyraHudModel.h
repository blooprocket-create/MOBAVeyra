// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Battleground/VeyraBattlegroundTypes.h"
#include "Containers/Array.h"
#include "Content/VeyraContentId.h"
#include "Misc/Optional.h"
#include "Slots/VeyraAbilitySlot.h"
#include "Statuses/VeyraStatusTypes.h"
#include "Teams/VeyraTeam.h"

class AActor;
class AVeyraPlayerState;
class UWorld;

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

	/** Seconds the empowerment it cast still waits for the next basic attack; 0 when none waits (Combat Bible §17). */
	double EmpoweredSeconds = 0.0;
};

/** The player's own panel. */
struct FVeyraHudPlayer
{
	FVeyraContentId Vanguard;
	int32 Level = 0;

	/** XP toward the next level, whole for display (the stored value keeps its fraction), and what that level needs; both 0 at the cap. */
	int32 Experience = 0;
	int32 ExperienceToNextLevel = 0;

	/** Gold, whole for display (Economy & Progression Bible §1: the UI rounds only for display). */
	int32 Gold = 0;

	int32 UnspentSkillPoints = 0;
	FVeyraHudVitals Vitals;

	/** Whether the Vanguard is dead, and the seconds until it returns (Economy & Progression Bible §14). */
	bool bDead = false;
	double RespawnSeconds = 0.0;

	/** Q, W, E and R, in order. */
	TArray<FVeyraHudSlot> Slots;

	/** The Vanguard's passive, as its definition names it; invalid when it has none. */
	FVeyraContentId Passive;
};

/** What a structure's bar says about it (Battleground Bible §5, §10, §18). */
struct FVeyraHudStructure
{
	EVeyraStructureKind Kind = EVeyraStructureKind::LaneSpire;

	/** Whether it cannot be damaged yet, because a structure before it stands. */
	bool bInvulnerable = false;

	/** Seconds until a destroyed inhibitor rebuilds; 0 when none is due. */
	double RebuildSeconds = 0.0;
};

/** One team's Team Flux on the HUD (ADR-011 §10). */
struct FVeyraHudTeamFlux
{
	EVeyraTeam Team = EVeyraTeam::None;
	double Active = 0.0;
	double Permanent = 0.0;

	/** The Health and damage fraction its Fluxborn gain from it. */
	double FluxbornBonus = 0.0;

	/** Seconds left on each temporary grant still counting, soonest first. */
	TArray<double> TemporarySeconds;
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

	/** What Unit's bar says about it as a structure at ServerNow; nothing when it is not one. */
	VEYRAUI_API TOptional<FVeyraHudStructure> StructureOf(const AActor& Unit, double ServerNow);

	/** Participant's panel at ServerNow, in server gameplay time. */
	VEYRAUI_API FVeyraHudPlayer DescribePlayer(const AVeyraPlayerState& Participant, double ServerNow);

	/** Each team's Team Flux at ServerNow, as the replicated Flux state holds it; empty before it arrives. */
	VEYRAUI_API TArray<FVeyraHudTeamFlux> DescribeTeamFlux(const UWorld* World, double ServerNow);
}
