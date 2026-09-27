// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "UObject/ObjectMacros.h"

#include "VeyraTuningProvenance.generated.h"

/**
 * Where a tuning record's values come from (ADR-008 §7). Canon gives no numbers for most Vanguard
 * kits, so values drafted by the implementer are marked Provisional until the author reviews them.
 * The marker is data about the values; no gameplay code reads it.
 */
UENUM()
enum class EVeyraTuningProvenance : uint8
{
	/** Drafted for playtesting; the author has not reviewed it. */
	Provisional,
	/** Stated by a design bible. */
	Canon,
	/** Drafted, then reviewed and kept by the author. */
	Reviewed,
};
