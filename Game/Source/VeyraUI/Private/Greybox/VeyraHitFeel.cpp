// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Greybox/VeyraHitFeel.h"

namespace
{
	/** What is left of a kick At seconds into its Seconds: 1 at its start, falling to 0, faster at first. */
	double FadeOf(double At, double Seconds)
	{
		if (Seconds <= 0.0 || At < 0.0 || At >= Seconds)
		{
			return 0.0;
		}
		return FMath::Square(1.0 - At / Seconds);
	}
}

double VeyraHitFeel::AmplitudeOf(EVeyraCombatCueKind Kind, double Amount, double MaxHealth, double HeavyShare, double HitAmplitude, double DeathAmplitude,
	double Scale)
{
	if (Kind == EVeyraCombatCueKind::Death)
	{
		return DeathAmplitude * Scale;
	}
	if (Kind == EVeyraCombatCueKind::Hit && MaxHealth > 0.0 && Amount >= HeavyShare * MaxHealth)
	{
		return HitAmplitude * Scale;
	}
	return 0.0;
}

FVector VeyraHitFeel::OffsetAt(double Amplitude, double At, double Seconds, double Frequency)
{
	const double Turn = UE_DOUBLE_TWO_PI * Frequency * At;
	return FVector(0.0, FMath::Sin(Turn), FMath::Cos(Turn)) * (Amplitude * FadeOf(At, Seconds));
}

bool VeyraHitFeel::Outshakes(double Amplitude, double At, double Seconds, double Fresh)
{
	return Amplitude * FadeOf(At, Seconds) >= Fresh;
}
