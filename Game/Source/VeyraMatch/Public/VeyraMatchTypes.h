// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "UObject/ObjectMacros.h"

#include "VeyraMatchTypes.generated.h"

/**
 * The in-match stages the server runs (Match Flow Bible §1). Champion select comes before and
 * resolution after; both arrive with the systems they need. Pause is not a phase: it freezes
 * whichever phase is running.
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
};

VEYRAMATCH_API const TCHAR* LexToString(EVeyraOrderRejection Rejection);
