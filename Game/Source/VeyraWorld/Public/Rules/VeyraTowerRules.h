// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/ArrayView.h"

class AActor;

/** A unit a tower could shoot now: a living enemy within its range. */
struct FVeyraTowerCandidate
{
	const AActor* Unit = nullptr;
	bool bVanguard = false;

	/** Edge to edge, in units (Combat Bible §40). */
	double Distance = 0.0;

	/** Breaks ties between equally near units, so every run resolves them alike. */
	uint32 StableId = 0;
};

/** What a tower shoots, and whether it holds Vanguard priority. */
struct FVeyraTowerChoice
{
	const AActor* Target = nullptr;
	bool bPriority = false;
};

/**
 * Lane Spires' and base-defense towers' shared targeting and ramp (Combat Bible §33; Battleground
 * Bible §19; ADR-011 §8). No world: the tower supplies what is in range.
 */
namespace VeyraTowerRules
{
	/**
	 * The tower's next target. Current is what it shoots now, bCurrentPriority whether it holds
	 * Vanguard priority, Claimant an enemy Vanguard that just damaged a defending Vanguard with both in
	 * range (null for none), and InRange every candidate. In order:
	 * - a valid priority target keeps priority: a later attacker does not steal it;
	 * - otherwise a claimant in range takes priority;
	 * - otherwise a valid current target is kept;
	 * - otherwise the nearest Fluxborn, then the nearest Vanguard.
	 * A target is valid while it is among the candidates: priority ends when it leaves range or dies.
	 */
	VEYRAWORLD_API FVeyraTowerChoice Choose(const AActor* Current, bool bCurrentPriority, const AActor* Claimant, TConstArrayView<FVeyraTowerCandidate> InRange);

	/**
	 * Ramp stacks for a shot at Target (Combat Bible §33): one more than the last shot's when the same
	 * Vanguard was shot last, up to MaxStacks; 0 for a new target or anything that is not a Vanguard.
	 */
	VEYRAWORLD_API int32 NextRampStacks(const AActor* LastTarget, int32 LastStacks, const AActor* Target, bool bTargetIsVanguard, int32 MaxStacks);

	/** The damage multiplier Stacks give: 1 plus PerShot for each. */
	VEYRAWORLD_API double RampMultiplier(int32 Stacks, double PerShot);
}
