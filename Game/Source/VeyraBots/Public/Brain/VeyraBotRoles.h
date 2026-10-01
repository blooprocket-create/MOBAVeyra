// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Tuning/VeyraBotsTuning.h"

/** How a team of bots shares its places (ADR-038 §5). No world: the bot subsystem gathers the facts. */
namespace VeyraBotRoles
{
	/**
	 * Deals a team's places to its bots: Places[I] is the role of the team's I-th place, and Preferences[I] the roles
	 * bot I plays, best first. The Jungle places go first, then the others in order, each to the bot not yet placed
	 * that ranks its role best; a bot that names the role not ranks it last, and ties go to the earlier bot. Returns,
	 * for each bot, the index of the place it takes. Places and Preferences are as many.
	 */
	VEYRABOTS_API TArray<int32> Deal(TConstArrayView<EVeyraBotRole> Places, TConstArrayView<TArray<EVeyraBotRole>> Preferences);
}
