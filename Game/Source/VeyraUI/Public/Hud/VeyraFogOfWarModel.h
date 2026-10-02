// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Math/Box2D.h"
#include "Teams/VeyraTeam.h"

class UWorld;
struct FVeyraSeenGround;

/** A run of neighbouring unseen cells in one row of a side's seen ground: cells First to Last of Row. */
struct FVeyraUnseenRun
{
	int32 Row = 0;
	int32 First = 0;
	int32 Last = 0;
};

/**
 * Fog of war on the client (ADR-054 §3): the ground the viewer's side does not see, from the seen ground Vision sends
 * that side alone. Presentation only.
 */
namespace VeyraFogOfWar
{
	/** The viewer's own side's seen ground on this machine, once it has arrived; null before, or with no side. */
	VEYRAUI_API const FVeyraSeenGround* OwnGround(const UWorld& World, EVeyraTeam Viewer);

	/** The unseen cells of Ground, row by row, as runs of neighbours: one shape each to darken. */
	VEYRAUI_API TArray<FVeyraUnseenRun> UnseenRuns(const FVeyraSeenGround& Ground);

	/** The ground Run covers, in world units. */
	VEYRAUI_API FBox2D BoundsOf(const FVeyraSeenGround& Ground, const FVeyraUnseenRun& Run);

	/** A value that changes whenever Ground does, so it is redrawn only then; 0 for none. */
	VEYRAUI_API uint32 SignatureOf(const FVeyraSeenGround* Ground);
}