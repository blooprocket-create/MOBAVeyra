// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "CoreMinimal.h"
#include "VeyraMatchTypes.h"

struct FVeyraVanguardDefinition;

/** Rules for how a match is played and ended, as pure functions the game mode applies. */
namespace VeyraMatchRules
{
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
}
