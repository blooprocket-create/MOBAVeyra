// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "UObject/ObjectMacros.h"

#include "VeyraMatchTypes.generated.h"

/**
 * The in-match stages the server runs (Match Flow Bible §1). Champion select comes before, and the
 * results screen after the end; both arrive with the systems they need. Pause is not a phase: it
 * freezes whichever phase is running.
 */
UENUM()
enum class EVeyraMatchPhase : uint8
{
	/** Waiting for the required participants and for the map, up to the loading timeout. */
	Loading,
	/** Fountain preparation: the match clock has not started. */
	Preparation,
	/** The match clock runs from 0:00. */
	Live,
	/** The match is over: its result is decided and it takes no more orders or players. */
	Ended,
};

/** Why the server refused a player's order. None means it was accepted. */
UENUM()
enum class EVeyraOrderRejection : uint8
{
	None,
	/** The player sent orders faster than the tuned rate. */
	TooFrequent,
	/** The match is not in a phase that takes this order. */
	WrongPhase,
	/** The match is paused (Match Flow Bible §10.2). */
	Paused,
	/** The player has no Vanguard in the world. */
	NoVanguard,
	/** The order's values are not usable, for example not finite. */
	InvalidOrder,
	/** The destination is not on or near walkable ground. */
	Unreachable,
	/** The target is not a living enemy unit, or the Vanguard has no basic attack. */
	CannotAttack,
};

VEYRAMATCH_API const TCHAR* LexToString(EVeyraOrderRejection Rejection);
