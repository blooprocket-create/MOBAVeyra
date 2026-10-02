// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Units/VeyraUnit.h"

class AActor;

/** A unit under the cursor, as the player's side sees it. */
struct FVeyraCursorUnit
{
	AActor* Actor = nullptr;
	EVeyraUnitKind Kind = EVeyraUnitKind::Vanguard;
	bool bHostile = false;
};

/**
 * Which unit under the cursor an order or a cast names (Settings Bible §1.4, §1.5; ADR-041 §3). Units are
 * nearest the camera first. Each pick only names a unit; the server judges whether it may be targeted.
 */
namespace VeyraCursorPicks
{
	/** A direct attack's target: the first hostile unit; with Target Vanguards Only, the first hostile Vanguard. */
	VEYRAMATCH_API AActor* Enemy(TConstArrayView<FVeyraCursorUnit> Under, bool bVanguardsOnly);

	/** The unit a cast names: the first one; with Target Vanguards Only, the first Vanguard of either side. */
	VEYRAMATCH_API AActor* ForCast(TConstArrayView<FVeyraCursorUnit> Under, bool bVanguardsOnly);

	/** Whether an allied Vanguard is under the cursor, so Smart Self-Cast leaves the cast to it. */
	VEYRAMATCH_API bool HasAlliedVanguard(TConstArrayView<FVeyraCursorUnit> Under);
}
