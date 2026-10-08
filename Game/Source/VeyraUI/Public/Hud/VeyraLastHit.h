// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Array.h"

/** How the last-hit cue shows an enemy Fluxborn's or creature's bar (ADR-065 §6, ADR-071 §5). */
enum class EVeyraLastHitStage : uint8
{
	/** Out of reach of one attack, even counting what else is hitting it. */
	None,
	/** One attack started now would land as it falls into reach, at the rate it is losing Health: get ready. */
	Ready,
	/** At or below one attack: an attack now kills it, unless something else does first. */
	Now,
};

/**
 * The Health a unit has been seen to lose, for the last-hit cue's Ready stage: its Health alone, never its shields, which
 * the attack must break before its Health falls (ADR-071 §5). Presentation only: the server decides who lands the last
 * hit.
 */
struct VEYRAUI_API FVeyraHealthLoss
{
	/**
	 * Takes the unit's Health at At: what it fell since the last sample is noted as lost, and a heal only moves the mark it
	 * falls from. Losses older than KeepSeconds before At are forgotten.
	 */
	void Sample(double At, double Health, double KeepSeconds);

	/** Notes Amount lost at At, forgetting losses older than KeepSeconds before it. */
	void Note(double At, double Amount, double KeepSeconds);

	/** The Health lost a second over the WindowSeconds before Now; 0 for none, or a window of 0. */
	double PerSecond(double Now, double WindowSeconds) const;

private:
	struct FLoss
	{
		double At = 0.0;
		double Amount = 0.0;
	};
	TArray<FLoss> Losses;

	/** The Health at the last sample; negative before the first. */
	double LastHealth = -1.0;
};

/** The last-hit cue's rules (ADR-065 §6, ADR-071 §5). */
namespace VeyraLastHit
{
	/**
	 * Which stage a unit with Health shows for an attack expected to take Hit: Now at or below Hit; Ready while its
	 * Health would fall to Hit in the LeadSeconds an attack takes to land, losing LossPerSecond; None otherwise, and
	 * for an attack that takes nothing. A unit nothing else is hitting goes straight from None to Now.
	 */
	VEYRAUI_API EVeyraLastHitStage StageOf(double Health, double Hit, double LossPerSecond, double LeadSeconds);

	/**
	 * How long an attack started now takes to land: its windup, then a projectile's flight over Distance at
	 * ProjectileSpeed; a melee attack (a speed of 0) lands as its windup ends.
	 */
	VEYRAUI_API double LeadSecondsOf(double WindupSeconds, double Distance, double ProjectileSpeed);
}
