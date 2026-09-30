// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "CoreMinimal.h"
#include "Join/VeyraMatchAssignment.h"
#include "VeyraMatchTypes.h"

struct FVeyraRespawnTuning;
struct FVeyraVanguardDefinition;

/** Rules for how a match is played and ended, as pure functions the game mode applies. */
namespace VeyraMatchRules
{
	/**
	 * Seconds from a Vanguard's death at Level to its respawn, with the match clock at
	 * MatchClockSeconds (Economy & Progression Bible §14): the level's timer, lengthened by a fraction
	 * for each minute past the curve's start, up to its cap. 0 respawns at once.
	 */
	VEYRAMATCH_API double RespawnDelaySeconds(int32 Level, double MatchClockSeconds, const FVeyraRespawnTuning& Respawn);

	/**
	 * Whether a player may end the match now as its custom match's host (Custom Matches Bible §4;
	 * ADR-010 §7). Only practice has a host; only the host may end it; an ended match stays ended.
	 */
	VEYRAMATCH_API EVeyraEndCustomMatchRefusal CheckEndCustomMatch(EVeyraMatchRules Rules, EVeyraMatchPhase Phase, bool bRequesterIsHost);

	/** Whether matches under Rules have a host, who may end them: practice and custom (ADR-021 §3). */
	VEYRAMATCH_API bool HasHost(EVeyraMatchRules Rules);

	/**
	 * Whether a match under Rules can be won: a standard match always, a custom match when its host left
	 * victory on, practice never (ADR-011 §14; ADR-021 §3).
	 */
	VEYRAMATCH_API bool HasVictory(EVeyraMatchRules Rules, const TOptional<FVeyraCustomSettings>& Custom);

	/** Whether dead Vanguards may buy back: standard and custom matches, not practice (ADR-020; ADR-021 §3). */
	VEYRAMATCH_API bool AllowsBuyback(EVeyraMatchRules Rules);

	/**
	 * Whether the Prime Well falling ends the match with a winner: in a match that can be won, while it
	 * is live. An ended match stays ended.
	 */
	VEYRAMATCH_API bool DoesPrimeWellWin(bool bHasVictory, EVeyraMatchPhase Phase);

	/**
	 * Why a server may not host a participant as Vanguard; empty when it may. Definition is what
	 * Vanguards.json defines for it, or null. A Shipping server (bShipping) hosts only Playable
	 * Vanguards (ADR-010 §6).
	 */
	VEYRAMATCH_API FString CheckAssignedVanguard(const FVeyraContentId& Vanguard, const FVeyraVanguardDefinition* Definition, bool bShipping);

	/**
	 * Why a server may not give a participant these starting Flux Spells; empty when it may. Each slot
	 * holds a spell of Roster or is empty (an invalid ID), no spell twice, and no more slots than a
	 * Vanguard has (ADR-015 §5).
	 */
	VEYRAMATCH_API FString CheckAssignedFluxSpells(TConstArrayView<FVeyraContentId> Spells, TConstArrayView<FVeyraContentId> Roster);
}
