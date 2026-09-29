// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Ledger/VeyraFluxLedger.h"

void FVeyraFluxLedger::Add(const FVeyraFluxGrantTuning& Grant, double Now)
{
	if (Grant.Duration == EVeyraFluxDuration::Permanent)
	{
		Permanent += Grant.Amount;
	}
	else
	{
		Temporary.Add({ Grant.Amount, Now + Grant.DurationSeconds });
	}
}

bool FVeyraFluxLedger::Expire(double Now)
{
	return Temporary.RemoveAll([Now](const FVeyraTemporaryFlux& Grant) { return Grant.ExpiresAt <= Now; }) > 0;
}

double FVeyraFluxLedger::Active(double Now) const
{
	double Total = Permanent;
	for (const FVeyraTemporaryFlux& Grant : Temporary)
	{
		Total += Grant.ExpiresAt > Now ? Grant.Amount : 0.0;
	}
	return Total;
}

TOptional<double> FVeyraFluxLedger::NextExpiry() const
{
	TOptional<double> Next;
	for (const FVeyraTemporaryFlux& Grant : Temporary)
	{
		Next = Next.IsSet() ? FMath::Min(Next.GetValue(), Grant.ExpiresAt) : Grant.ExpiresAt;
	}
	return Next;
}

namespace VeyraFlux
{
FVeyraFluxbornStrength StrengthFor(double Active, const FVeyraFluxbornScalingTuning& Scaling)
{
	FVeyraFluxbornStrength Strength;
	if (Scaling.StepFlux > 0.0 && Active > 0.0)
	{
		const double Steps = FMath::FloorToDouble(Active / Scaling.StepFlux);
		Strength.HealthMultiplier += Steps * Scaling.HealthPerStep;
		Strength.DamageMultiplier += Steps * Scaling.DamagePerStep;
	}
	return Strength;
}
}
