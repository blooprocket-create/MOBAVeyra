// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "HAL/Platform.h"
#include "UObject/ObjectMacros.h"

#include "VeyraDamageTypes.generated.h"

// Value types of the canonical damage pipeline (Combat Bible §25). They are plain data: no world,
// no Gameplay Ability System types, and all arithmetic in double. The default values below are the
// identities of their operations (multiply by 1, subtract 0), not tuning.

/** The three primary damage types (Combat Bible §2). Reflected so tuning can name them. */
UENUM()
enum class EVeyraDamageType : uint8
{
	Physical,
	Magic,
	/** True damage. Unreal forbids an enum value named "True", so the name carries its suffix. */
	TrueDamage,
};

/** One typed component of a damage event. Components resolve independently (Combat Bible §25). */
struct FVeyraDamageComponent
{
	EVeyraDamageType Type = EVeyraDamageType::Physical;
	double Amount = 0.0;
};

/** One damage event can carry at most one component per type. */
using FVeyraDamageComponents = TArray<FVeyraDamageComponent, TInlineAllocator<3>>;

/** Step 2 of §25: the raw damage event before any modifier. */
struct FVeyraRawDamageEvent
{
	FVeyraDamageComponents Components;
};

/** An attacker's penetration against one resistance (Combat Bible §3). */
struct FVeyraPenetration
{
	/** Subtracted from positive resistance after percentage penetration. */
	double Flat = 0.0;

	/** The fraction of positive resistance kept by percentage penetration: Π(1 − x) over sources. */
	double Retained = 1.0;
};

/** Resistance reduction applied to one of a target's resistances (Combat Bible §3). */
struct FVeyraResistanceReduction
{
	/** Subtracted from resistance first; it may push resistance below 0. */
	double Flat = 0.0;

	/** The fraction of positive resistance kept by percentage reduction: Π(1 − x) over sources. */
	double Retained = 1.0;
};

/** The attacker's side of a damage event. */
struct FVeyraAttackerOffence
{
	/** Generic Damage Amplification (§15): the product of every source's 1 + x. */
	double OutgoingDamageMultiplier = 1.0;

	FVeyraPenetration PhysicalPenetration;
	FVeyraPenetration MagicPenetration;
};

/** The defender's side of a damage event. */
struct FVeyraDefenderDefence
{
	/** Armor and Magic Resistance after the §41 stat pipeline and before §3 reduction. */
	double Armor = 0.0;
	double MagicResist = 0.0;

	FVeyraResistanceReduction ArmorReduction;
	FVeyraResistanceReduction MagicResistReduction;

	/** Generic Damage Reduction (§15): the product of every source's 1 − x. */
	double IncomingDamageMultiplier = 1.0;
};

/**
 * The damage after the attacker's side (§25 steps 2–3), together with the attacker's penetration.
 * It is everything the attacker contributes, so a projectile can carry it after launch (§50).
 */
struct FVeyraDamagePayload
{
	FVeyraDamageComponents Components;
	FVeyraPenetration PhysicalPenetration;
	FVeyraPenetration MagicPenetration;
};

/** The damage after mitigation and target-side reduction (§25 steps 4–6), before absorption. */
struct FVeyraMitigatedDamage
{
	FVeyraDamageComponents Components;
};
