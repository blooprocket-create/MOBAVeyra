// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Math/Vector2D.h"
#include "Units/VeyraUnit.h"

class AActor;

/** A unit a Fluxborn could attack now: a living, damageable enemy within its acquisition radius. */
struct FVeyraFluxbornCandidate
{
	const AActor* Unit = nullptr;
	EVeyraUnitKind Kind = EVeyraUnitKind::Fluxborn;

	/** Edge to edge, in units (Combat Bible §40). */
	double Distance = 0.0;

	/** Whether it is within the Fluxborn's attack range now. */
	bool bInAttackRange = false;

	/** Breaks ties between equally near units, so every run resolves them alike. */
	uint32 StableId = 0;
};

/** What a Fluxborn attacks, and whether it answers aggression against an ally. */
struct FVeyraFluxbornChoice
{
	const AActor* Target = nullptr;
	bool bResponding = false;
};

/**
 * Fluxborn behaviour that needs no world (Battleground Bible §19; Combat Bible §33; ADR-011 §7).
 */
namespace VeyraFluxbornRules
{
	/**
	 * The Fluxborn's next target. Current is what it attacks now, bResponding whether it attacks
	 * that one for hurting an ally, Claimant an enemy Vanguard that just hurt a nearby allied
	 * Vanguard (null for none), bSiege whether it is a siege unit, and InRange every candidate. By
	 * rank, then distance, then stable ID:
	 * 1. answering aggression: a claimant, or a current target it is already answering;
	 * 2. for a siege unit, a structure within attack range;
	 * 3. an enemy Fluxborn;
	 * 4. an enemy structure;
	 * 5. an enemy Vanguard.
	 * A valid current target is kept unless a better rank is on offer. A target is valid while it is
	 * among the candidates: a chase ends when it leaves.
	 */
	VEYRAWORLD_API FVeyraFluxbornChoice Choose(const AActor* Current, bool bResponding, const AActor* Claimant, bool bSiege,
		TConstArrayView<FVeyraFluxbornCandidate> InRange);

	/**
	 * Where a Fluxborn at Location rejoins its lane after a chase: the index of the first waypoint
	 * ahead of the path segment nearest it, so it never walks back (ADR-011 §7). Waypoints run from
	 * its own base toward the enemy's; the last index when it is past them all.
	 */
	VEYRAWORLD_API int32 ResumeWaypoint(TConstArrayView<FVector2D> Waypoints, const FVector2D& Location);

	/**
	 * How far Location lies from the lane's path through Waypoints. A Fluxborn engages only what
	 * lies within its leash range of its lane, so a chase ends when the target draws it away.
	 */
	VEYRAWORLD_API double DistanceFromLane(TConstArrayView<FVector2D> Waypoints, const FVector2D& Location);
}
