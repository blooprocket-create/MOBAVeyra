// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Array.h"
#include "Pings/VeyraPingTypes.h"

struct FVeyraPingsTuning;

/** The pure rules of team pings (ADR-020 §2), tested without a world. */
namespace VeyraPings
{
	/**
	 * Whether a player who sent pings at SentAt may send another at Now: at most pings.maxPerWindow in
	 * any pings.windowSeconds. If so, records it. SentAt forgets what is older than the window. Real
	 * seconds, so a pause does not hold the count.
	 */
	VEYRAMATCH_API bool Allow(TArray<double>& SentAt, double Now, const FVeyraPingsTuning& Tuning);

	/** Forgets the pings a client has held for pings.keepSeconds or longer, at Now. */
	VEYRAMATCH_API void Forget(TArray<FVeyraReceivedPing>& Held, double Now, const FVeyraPingsTuning& Tuning);
}
