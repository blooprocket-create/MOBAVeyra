// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Rules/VeyraFluxWellRules.h"

#include "Tuning/VeyraWorldTuning.h"

namespace VeyraFluxWellRules
{
double DrainRate(int32 Count, const FVeyraFluxWellPresenceTuning& Presence)
{
	const int32 Counted = FMath::Min(Count, Presence.MaxCounted);
	return Counted <= 0 ? 0.0 : Presence.DrainPerSecond + Presence.DrainPerAdditional * (Counted - 1);
}

FVeyraPresenceDrain PresenceDrain(int32 CountA, int32 CountB, const FVeyraFluxWellPresenceTuning& Presence)
{
	const int32 A = FMath::Min(CountA, Presence.MaxCounted);
	const int32 B = FMath::Min(CountB, Presence.MaxCounted);
	FVeyraPresenceDrain Drain;
	if (A == B)
	{
		// Nobody there, or equal control: it stalls (§6).
		return Drain;
	}
	Drain.Side = A > B ? EVeyraTeam::A : EVeyraTeam::B;
	const int32 Lead = FMath::Abs(A - B);
	const bool bContested = A > 0 && B > 0;
	Drain.PerSecond = DrainRate(Lead, Presence) * (bContested ? Presence.ContestedFactor : 1.0);
	return Drain;
}

bool Regenerates(double Now, double LastWorkedAt, double IdleSeconds)
{
	return Now - LastWorkedAt >= IdleSeconds;
}
}
