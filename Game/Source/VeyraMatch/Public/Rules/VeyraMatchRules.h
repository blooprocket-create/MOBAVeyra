// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "CoreMinimal.h"
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

	/**
	 * Whether a destroyed Prime Well wins the match now (Battleground Bible §18; ADR-011 §13, §14):
	 * in a standard match that is live. Practice has no victory condition, and an ended match stays ended.
	 */
	VEYRAMATCH_API bool DoesPrimeWellWin(EVeyraMatchRules Rules, EVeyraMatchPhase Phase);

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
