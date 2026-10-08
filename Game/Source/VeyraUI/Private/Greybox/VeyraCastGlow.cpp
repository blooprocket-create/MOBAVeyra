// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Greybox/VeyraCastGlow.h"

double VeyraCastGlow::Step(double Glow, bool bHeld, double DeltaSeconds, double RiseSeconds, double FallSeconds)
{
	const double Seconds = bHeld ? RiseSeconds : FallSeconds;
	const double Change = Seconds > UE_KINDA_SMALL_NUMBER ? DeltaSeconds / Seconds : 1.0;
	return FMath::Clamp(Glow + (bHeld ? Change : -Change), 0.0, 1.0);
}

double VeyraCastGlow::Multiplier(double Glow, double Gain)
{
	return FMath::Lerp(1.0, Gain, FMath::Clamp(Glow, 0.0, 1.0));
}
