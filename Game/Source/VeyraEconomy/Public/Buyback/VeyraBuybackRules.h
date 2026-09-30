// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "VeyraBuybackRules.generated.h"

struct FVeyraBuybackTuning;

/** Why a buyback was refused (Economy & Progression Bible §15). None means it may be, or was, bought. */
UENUM()
enum class EVeyraBuybackRefusal : uint8
{
	None,
	/** The match offers none now: practice, before or after the live match, or paused (ADR-020 §3). */
	Unavailable,
	/** Before the match time buyback opens (§15: 10:00). */
	TooEarly,
	/** Only a Vanguard that has actually died buys back; none is bought ahead (§15). */
	Alive,
	/** Its personal cooldown runs from its last buyback (§15). */
	CoolingDown,
	NotEnoughGold,
};

VEYRAECONOMY_API const TCHAR* LexToString(EVeyraBuybackRefusal Refusal);

/** What the next buyback costs, and whether it may be bought: the server and the shop reach the same answer. */
struct FVeyraBuybackQuote
{
	double Cost = 0.0;
	EVeyraBuybackRefusal Refusal = EVeyraBuybackRefusal::None;

	bool operator==(const FVeyraBuybackQuote&) const = default;
};

/** Buyback's arithmetic (§15), from Economy.json's values. */
namespace VeyraBuyback
{
	/**
	 * Its cost at MatchSeconds for a Vanguard that has bought Purchases already: it rises with each whole
	 * minute past its opening and with each earlier purchase, independently.
	 */
	VEYRAECONOMY_API double Cost(double MatchSeconds, int32 Purchases, const FVeyraBuybackTuning& Tuning);

	/**
	 * The next buyback for a Vanguard, dead or not, holding Gold, at MatchSeconds, when its cooldown ends at
	 * ReadyAt; Now is in the same world time as ReadyAt.
	 */
	VEYRAECONOMY_API FVeyraBuybackQuote Quote(double MatchSeconds, double Now, bool bDead, double Gold, int32 Purchases, double ReadyAt,
		const FVeyraBuybackTuning& Tuning);
}
