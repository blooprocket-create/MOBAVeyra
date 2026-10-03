// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Input/VeyraOrderMark.h"
#include "Math/Color.h"
#include "Misc/Optional.h"

class UVeyraGreyboxSettings;

/** One frame of an order's mark: a ring on the ground. */
struct FVeyraOrderMarkRing
{
	FVector Centre = FVector::ZeroVector;
	double Radius = 0.0;
	FLinearColor Color = FLinearColor::Transparent;
};

/** The mark a local order leaves at once where it went, before the server answers (ADR-062 §6). */
namespace VeyraOrderMarks
{
	/**
	 * How Mark shows at Now, in this machine's real seconds: a ring that closes from OrderMarkStartRadius to
	 * OrderMarkEndRadius over OrderMarkSeconds, fading as it goes, on the ground ordered or around the edge of the
	 * unit an attack order named, in the move or the attack colour. Still (Reduce Interface Animation), it keeps its
	 * end radius and only fades. Unset once it has faded, or while its unit is gone or hidden.
	 */
	VEYRAUI_API TOptional<FVeyraOrderMarkRing> Describe(const FVeyraOrderMark& Mark, double Now, bool bStill, const UVeyraGreyboxSettings& Settings);
}
