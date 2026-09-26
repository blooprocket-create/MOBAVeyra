// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Damage/VeyraDamageResolver.h"

#include "Math/UnrealMathUtility.h"
#include "Misc/AssertionMacros.h"
#include "Tuning/VeyraCombatTuning.h"

namespace VeyraDamage
{
namespace
{
	// Damage is never negative (§25), so no multiplier may turn it into healing.
	double NonNegative(double Value)
	{
		return FMath::Max(0.0, Value);
	}

	double MitigateAgainst(double Amount, double Resistance, const FVeyraResistanceReduction& Reduction,
		const FVeyraPenetration& Penetration, const FVeyraResistanceTuning& Tuning)
	{
		return Amount * ResistanceDamageMultiplier(ResolveEffectiveResistance(Resistance, Reduction, Penetration), Tuning.MitigationConstant);
	}
}

FVeyraDamagePayload ApplySourceSide(const FVeyraRawDamageEvent& Raw, const FVeyraAttackerOffence& Offence)
{
	FVeyraDamagePayload Payload;
	Payload.PhysicalPenetration = Offence.PhysicalPenetration;
	Payload.MagicPenetration = Offence.MagicPenetration;

	const double Amplification = NonNegative(Offence.OutgoingDamageMultiplier);
	for (const FVeyraDamageComponent& Component : Raw.Components)
	{
		const double Amount = NonNegative(Component.Amount);
		// Generic amplification affects Physical and Magic Damage only (§15).
		const double Amplified = Component.Type == EVeyraDamageType::TrueDamage ? Amount : Amount * Amplification;
		Payload.Components.Add({ Component.Type, Amplified });
	}
	return Payload;
}

double ReduceResistance(double Resistance, const FVeyraResistanceReduction& Reduction)
{
	const double AfterFlat = Resistance - Reduction.Flat;
	return AfterFlat > 0.0 ? AfterFlat * NonNegative(Reduction.Retained) : AfterFlat;
}

double ResolveEffectiveResistance(double Resistance, const FVeyraResistanceReduction& Reduction, const FVeyraPenetration& Penetration)
{
	const double Reduced = ReduceResistance(Resistance, Reduction);
	if (Reduced <= 0.0)
	{
		return Reduced;
	}
	const double AfterPercentage = Reduced * NonNegative(Penetration.Retained);
	return FMath::Max(0.0, AfterPercentage - Penetration.Flat);
}

double ResistanceDamageMultiplier(double Resistance, double MitigationConstant)
{
	checkf(MitigationConstant > 0.0, TEXT("The mitigation constant must come from validated Combat tuning (got %f)."), MitigationConstant);
	if (Resistance >= 0.0)
	{
		return MitigationConstant / (MitigationConstant + Resistance);
	}
	return 1.0 + (-Resistance) / (MitigationConstant - Resistance);
}

FVeyraMitigatedDamage ApplyTargetSide(const FVeyraDamagePayload& Payload, const FVeyraDefenderDefence& Defence, const FVeyraResistanceTuning& Tuning)
{
	FVeyraMitigatedDamage Mitigated;
	const double Reduction = NonNegative(Defence.IncomingDamageMultiplier);
	for (const FVeyraDamageComponent& Component : Payload.Components)
	{
		const double Amount = NonNegative(Component.Amount);
		double Result = Amount;
		switch (Component.Type)
		{
		case EVeyraDamageType::Physical:
			Result = MitigateAgainst(Amount, Defence.Armor, Defence.ArmorReduction, Payload.PhysicalPenetration, Tuning) * Reduction;
			break;
		case EVeyraDamageType::Magic:
			Result = MitigateAgainst(Amount, Defence.MagicResist, Defence.MagicResistReduction, Payload.MagicPenetration, Tuning) * Reduction;
			break;
		case EVeyraDamageType::TrueDamage:
			// True Damage skips resistance and generic reduction (§25 steps 5–6).
			break;
		}
		Mitigated.Components.Add({ Component.Type, Result });
	}
	return Mitigated;
}
}
