// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Array.h"
#include "Content/VeyraContentId.h"

struct FVeyraWavesTuning;

/**
 * The Fluxborn wave schedule and composition as pure functions of their tuning (Battleground Bible
 * §17, §18; ADR-011 §7). Times are match-clock seconds, which a pause holds.
 */
namespace VeyraWaveRules
{
	/** The interval after a wave that spawns at Seconds: the one of the last phase begun by then. */
	VEYRAWORLD_API double IntervalAt(const FVeyraWavesTuning& Waves, double Seconds);

	/**
	 * When wave Index spawns (0 for the first). Each wave follows the one before it by the interval
	 * of the phase that one spawned in, so crossing a phase boundary never duplicates or skips one.
	 */
	VEYRAWORLD_API double WaveTime(const FVeyraWavesTuning& Waves, int32 Index);

	/** Whether wave Index, spawning at Seconds, brings siege units: every Nth wave, N from the phase at Seconds. */
	VEYRAWORLD_API bool HasSiege(const FVeyraWavesTuning& Waves, int32 Index, double Seconds);

	/**
	 * One lane's wave for one team, in the order its units leave the base: the extra units a downed
	 * enemy inhibitor adds (bInhibitorDown), the first kind of the ordinary units, the siege units
	 * (bSiege), then the rest of the ordinary units.
	 */
	VEYRAWORLD_API TArray<FVeyraContentId> Composition(const FVeyraWavesTuning& Waves, bool bSiege, bool bInhibitorDown);
}
