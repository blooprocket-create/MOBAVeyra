// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/** A generated body straining while it casts (ADR-072 §5): its glow surging as a cast holds it, and easing back after. */
namespace VeyraCastGlow
{
	/**
	 * How far Glow (0 at rest, 1 at full strain) has come after DeltaSeconds: toward 1 over RiseSeconds while a cast holds
	 * the body (bHeld), toward 0 over FallSeconds otherwise.
	 */
	VEYRAUI_API double Step(double Glow, bool bHeld, double DeltaSeconds, double RiseSeconds, double FallSeconds);

	/** What a Gain at full strain multiplies a thing by at Glow: 1 at rest, Gain at full strain, between them on the way. */
	VEYRAUI_API double Multiplier(double Glow, double Gain);
}
