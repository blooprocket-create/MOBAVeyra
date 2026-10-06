// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Feedback/VeyraCombatTextTypes.h"

/** A combat text number as this client received it, stamped by its own clock. */
struct FVeyraCombatTextArrival
{
	FVeyraCombatTextLine Line;
	double ReceivedAt = 0.0;
};

/** How combat text shows, as the player set it (Settings Bible §3.4; ADR-052 §1) and the HUD times it. */
struct FVeyraCombatTextOptions
{
	bool bDamageDealt = true;
	bool bDamageReceived = true;
	bool bHealing = true;
	bool bShielding = true;
	/** Gold the player earned (ADR-065 §4). */
	bool bGold = true;
	/** A crit's number stands out. */
	bool bCritEmphasis = true;
	/** Reduced density: quick numbers between the same two units merge into a running total. */
	bool bReduced = false;
	/** Reduced merges a number arriving within this many seconds of the last it would join. */
	double MergeSeconds = 0.0;
	/**
	 * At any density, Gold arriving within this many seconds of the last Gold at the same place joins it, so a kill and
	 * its bounty read as one figure.
	 */
	double GoldMergeSeconds = 0.0;
	/** How long a number rises and fades, in seconds. */
	double ShowSeconds = 0.0;
};

/** A number to draw now. */
struct FVeyraCombatTextShown
{
	TWeakObjectPtr<AActor> Unit;
	EVeyraCombatTextKind Kind = EVeyraCombatTextKind::DamageDealt;
	EVeyraDamageType DamageType = EVeyraDamageType::Physical;
	bool bCritical = false;
	double Amount = 0.0;
	/** How far through its showing it is: 0 as its latest part arrives, nearing 1 as it vanishes. */
	double Progress = 0.0;
	/** It shows at Where, not at a unit: Gold a fall earned (ADR-065 §4). */
	bool bFixed = false;
	FVector Where = FVector::ZeroVector;
};

/** The client's presentation of combat text, apart from the engine. */
namespace VeyraCombatTextView
{
	/**
	 * Drops the arrivals that have finished showing by Now, as Describe groups them: every part of a running total stays
	 * while the total shows, from its latest part. Arrivals of a kind the player turned off go at once.
	 */
	VEYRAUI_API void Forget(TArray<FVeyraCombatTextArrival>& Arrivals, double Now, const FVeyraCombatTextOptions& Options);

	/**
	 * The numbers to draw at Now, in the order they arrived: the kinds the player turned off are left out, and
	 * under Reduced density a number arriving within MergeSeconds of the last between the same two units, of the
	 * same kind and type, adds to its running total, which shows from its latest part. Gold arriving within
	 * GoldMergeSeconds of the last Gold at the same unit or place adds to it at any density. A number that rounds to
	 * nothing shows nothing.
	 */
	VEYRAUI_API TArray<FVeyraCombatTextShown> Describe(TConstArrayView<FVeyraCombatTextArrival> Arrivals, double Now, const FVeyraCombatTextOptions& Options);
}
