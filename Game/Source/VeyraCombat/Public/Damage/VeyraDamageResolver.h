// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Damage/VeyraDamageTypes.h"

struct FVeyraResistanceTuning;

/**
 * The canonical damage math, Combat Bible §25 steps 2–6. The only damage formulas in the game:
 * Gameplay Effect executions call these and never compute damage themselves (ADR-002). Steps 7–9
 * (invulnerability, shields, Temporary Health, Health) are in Absorption/VeyraAbsorptionLedger.h.
 */
namespace VeyraDamage
{
	/**
	 * Steps 2–3: applies the attacker's generic Damage Amplification to Physical and Magic
	 * components (§15) and snapshots the attacker's penetration. True Damage is not amplified.
	 */
	VEYRACOMBAT_API FVeyraDamagePayload ApplySourceSide(const FVeyraRawDamageEvent& Raw, const FVeyraAttackerOffence& Offence);

	/**
	 * The target's resistance after reduction (§3 order steps 1–2): flat reduction, which may go below
	 * 0, then percentage reduction, which applies only while the resistance is positive. This is the
	 * target's actual defensive stat.
	 */
	VEYRACOMBAT_API double ReduceResistance(double Resistance, const FVeyraResistanceReduction& Reduction);

	/**
	 * The resistance one attacker's damage meets (§3 order steps 1–4): reduction, then percentage
	 * penetration and flat penetration, which only bypass positive resistance and never push it
	 * below 0.
	 */
	VEYRACOMBAT_API double ResolveEffectiveResistance(double Resistance, const FVeyraResistanceReduction& Reduction,
		const FVeyraPenetration& Penetration);

	/**
	 * The fraction of damage taken at a resistance (§3): K / (K + R) for R ≥ 0, and
	 * 1 + (−R) / (K − R) for R < 0, which is canon's 2 − K / (K − R) written so that it never
	 * exceeds its 2× limit.
	 */
	VEYRACOMBAT_API double ResistanceDamageMultiplier(double Resistance, double MitigationConstant);

	/**
	 * Steps 4–6: Armor mitigates Physical and Magic Resistance mitigates Magic, then the target's
	 * generic Damage Reduction applies (§15). True Damage skips both. Results are never negative and
	 * are not rounded.
	 */
	VEYRACOMBAT_API FVeyraMitigatedDamage ApplyTargetSide(const FVeyraDamagePayload& Payload, const FVeyraDefenderDefence& Defence,
		const FVeyraResistanceTuning& Tuning);
}
