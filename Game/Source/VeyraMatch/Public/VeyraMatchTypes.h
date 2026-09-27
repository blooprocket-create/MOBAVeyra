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

/** Which rules a match plays by (ADR-010 §7, §9). */
UENUM()
enum class EVeyraMatchRules : uint8
{
	/** A matchmade or developer match. */
	Standard,
	/** Solo Custom practice: its host alone, open-ended, and ended by the host (Custom Matches Bible §1, §4). */
	Practice,
};

/** Why the server refused to end a custom match. None means it ended. */
UENUM()
enum class EVeyraEndCustomMatchRefusal : uint8
{
	None,
	/** The match is not a custom match, so only its own rules end it. */
	NotCustomMatch,
	/** Only the custom match's host may end it (Custom Matches Bible §4). */
	NotHost,
	/** The match has already ended. */
	AlreadyEnded,
};

VEYRAMATCH_API const TCHAR* LexToString(EVeyraEndCustomMatchRefusal Refusal);
