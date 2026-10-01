// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Companions/VeyraCompanion.h"

class AActor;

/** One enemy a companion might fight (ADR-034 §4). */
struct FVeyraCompanionCandidate
{
	const AActor* Unit = nullptr;

	/** Whether its owner fought it lately. */
	bool bOwnersTarget = false;

	bool bVanguard = false;

	/** Edge to edge from the companion. */
	double Distance = 0.0;

	/** Breaks a tie in distance. */
	uint32 StableId = 0;

	/** Whether it holds a Designation from the companion's owner (ADR-037 §5). */
	bool bDesignated = false;
};

/** Whom a companion fights (ADR-034 §4). No world: the controller gathers the facts. */
namespace VeyraCompanionRules
{
	/**
	 * An enemy its owner Designated comes first (ADR-037 §5). Following, it fights only that, or an enemy its owner
	 * fought lately. Holding a point, it fights one its owner fought lately, else an enemy Vanguard, else any enemy.
	 * Anchored, one its owner fought lately, else any enemy. Nearest first, then the lowest ID; it keeps Current
	 * while Current is a candidate of the best rank there is. Null when none qualifies.
	 */
	VEYRAABILITIES_API const AActor* Choose(EVeyraCompanionMode Mode, const AActor* Current, TConstArrayView<FVeyraCompanionCandidate> Candidates);
}
