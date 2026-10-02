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

	/** The first allied Vanguard, the player's own among them: the only unit an ally's cast may name. */
	VEYRAMATCH_API AActor* Ally(TConstArrayView<FVeyraCursorUnit> Under);

	/**
	 * The unit a cast names, of the side its ability targets: for one that may land on an ally (bNamesAlly), the
	 * first allied Vanguard, whatever Target Vanguards Only says; for any other, as an attack names its target.
	 */
	VEYRAMATCH_API AActor* ForCast(TConstArrayView<FVeyraCursorUnit> Under, bool bVanguardsOnly, bool bNamesAlly);

	/**
	 * Whether Smart Self-Cast names Caster for a cast of CastRange (Settings Bible §1.5): unless the first allied
	 * Vanguard under the cursor is a valid target for it, alive and within range, as the server judges it.
	 */
	VEYRAMATCH_API bool SmartSelfCasts(const AActor& Caster, TConstArrayView<FVeyraCursorUnit> Under, double CastRange);
}
