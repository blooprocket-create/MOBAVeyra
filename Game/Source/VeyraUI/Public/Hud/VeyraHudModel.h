// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Wells/VeyraFluxWell.h"
#include "Battleground/VeyraBattlegroundTypes.h"
#include "Containers/Array.h"
#include "Content/VeyraContentId.h"
#include "Misc/Optional.h"
#include "Slots/VeyraAbilitySlot.h"
#include "Tools/VeyraVisionToolComponent.h"
#include "Statuses/VeyraStatusTypes.h"
#include "Teams/VeyraTeam.h"
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
	/** Its stacks, as Cadence's or Hex's count (ADR-018 §2). */
	int32 Stacks = 1;
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

/** One inventory slot on the HUD's item bar, used by its key (ADR-012 §1). */
struct FVeyraHudItemSlot
{
	EVeyraAbilitySlot Slot = EVeyraAbilitySlot::Item1;

	/** Invalid when the slot is empty. */
	FVeyraContentId Item;
	int32 Count = 0;

	/** A refillable consumable's charges left, which the bar always shows; unset for other items. */
	TOptional<int32> Charges;

	/** A Quest Item's progress and threshold (ADR-025 §3); unset for other items. */
	TOptional<FIntPoint> Quest;

	/** The Current and the Reserve its Attunements keep, whole (ADR-025 §7); unset for an item that keeps none. */
	TOptional<int32> Current;
	TOptional<int32> Reserve;

	/** Seconds until its Active is ready; 0 when it is, or it has none. */
	double CooldownSeconds = 0.0;
};

/** One Flux Spell slot on the HUD (ADR-015 §7): locked with the permanent Flux it needs, ready, or cooling down. */
struct FVeyraHudSpellSlot
{
	EVeyraAbilitySlot Slot = EVeyraAbilitySlot::Spell1;

	/** Invalid when the slot is empty. */
	FVeyraContentId Spell;

	bool bLocked = false;

	/** The permanent Team Flux that unlocks the slot (Battleground Bible §14). */
	double UnlockFlux = 0.0;

	/** Seconds until it is ready; 0 when it is. */
	double CooldownSeconds = 0.0;
};

/**
 * The vision-tool slot on the HUD (ADR-016 §8): the tool in it; for Persistent Ward, its charges and
 * when the next comes back; for the others, their cooldown.
 */
struct FVeyraHudVisionTool
{
	bool bPresent = false;
	EVeyraVisionTool Tool = EVeyraVisionTool::PersistentWard;

	/** Seconds until Sweeper or Quick Sight is ready; 0 when it is. */
	double CooldownSeconds = 0.0;

	int32 WardCharges = 0;
	int32 MaxWardCharges = 0;

	/** Seconds until the next charge comes back; 0 while none is coming. */
	double NextChargeSeconds = 0.0;
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

	/** Whether a Recall channel runs (ADR-012 §8): the seconds it has left, and how much of it has passed, from 0 to 1. */
	bool bRecalling = false;
	double RecallSeconds = 0.0;
	double RecallProgress = 0.0;

	/** Q, W, E and R, in order. */
	TArray<FVeyraHudSlot> Slots;

	/** The inventory's slots, 1 to 6, in order. */
	TArray<FVeyraHudItemSlot> Items;

	/** The two Flux Spell slots, in order. */
	TArray<FVeyraHudSpellSlot> Spells;

	FVeyraHudVisionTool VisionTool;

	/** Purchases waiting for the fountain (Economy & Progression Bible §11). */
	int32 PendingPurchases = 0;

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

/** What a Flux Well's bar says about it (Battleground Bible §6; ADR-014 §4). */
struct FVeyraHudFluxWell
{
	EVeyraFluxWellState State = EVeyraFluxWellState::Closed;

	/** Seconds until it opens, closed or respawning; 0 while open. */
	double OpensInSeconds = 0.0;
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
/** A presence ping on the HUD (ADR-016 §8): a ring over the fog circle an enemy is present in. */
struct FVeyraHudPing
{
	FVector2D Centre = FVector2D::ZeroVector;
	double Radius = 0.0;

	/** How much of it is left, from 1 as it arrives to 0 as the next would come. */
	double Fade = 0.0;
};

/** What the viewer's side's vision tells it (ADR-016 §8): its presence pings and Sweeper's outlines. */
struct FVeyraHudVision
{
	TArray<FVeyraHudPing> Pings;

	/** Where each outlined enemy stands, or was last covered. */
	TArray<FVector> Outlines;
};

namespace VeyraHud
{
	/**
	 * What a unit's bars and statuses show to a viewer on Viewer's side: a placed marker that presents as
	 * its owner shows its owner's participant to its owner's enemies (ADR-030 §5); its owner's side, and
	 * a viewer on no side, see it for what it is; anything else shows itself.
	 */
	VEYRAUI_API const AActor& PresentedUnitOf(const AActor& Unit, EVeyraTeam Viewer);

	/**
	 * Unit's bars as Viewer's side sees them, from its Ability System Component and shields; nothing when it
	 * has neither Health nor an Ability System Component.
	 */
	VEYRAUI_API TOptional<FVeyraHudVitals> VitalsOf(const AActor& Unit, EVeyraTeam Viewer);

	/**
	 * Unit's statuses as Viewer's side sees them, in the order they were applied, each with its time left at
	 * ServerNow, in server gameplay time.
	 */
	VEYRAUI_API TArray<FVeyraHudStatus> StatusesOf(const AActor& Unit, double ServerNow, EVeyraTeam Viewer);

	/** What Unit's bar says about it as a structure at ServerNow; nothing when it is not one. */
	VEYRAUI_API TOptional<FVeyraHudStructure> StructureOf(const AActor& Unit, double ServerNow);

	/** What Unit's bar says about it as a Flux Well at ServerNow; nothing when it is not one. */
	VEYRAUI_API TOptional<FVeyraHudFluxWell> FluxWellOf(const AActor& Unit, double ServerNow);

	/** Unit's species as a jungle creature; nothing when it is not one (ADR-014 §2). */
	VEYRAUI_API TOptional<FVeyraContentId> SpeciesOf(const AActor& Unit);

	/** Participant's panel at ServerNow, in server gameplay time. */
	VEYRAUI_API FVeyraHudPlayer DescribePlayer(const AVeyraPlayerState& Participant, double ServerNow);

	/** Each team's Team Flux at ServerNow, as the replicated Flux state holds it; empty before it arrives. */
	VEYRAUI_API TArray<FVeyraHudTeamFlux> DescribeTeamFlux(const UWorld* World, double ServerNow);

	/** What Viewer's side's vision tells it at ServerNow; empty before its team state arrives. */
	VEYRAUI_API FVeyraHudVision DescribeVision(const UWorld* World, EVeyraTeam Viewer, double ServerNow);
}
