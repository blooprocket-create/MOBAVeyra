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

/**
 * How a damage event is delivered, which decides whether it can damage a structure (Combat Bible §33,
 * §55; ADR-011 §5). It stands in for canon's Structure Attack and Structure Projectile tags, which
 * are on Combat §2's open descriptive-tag list and so are not added.
 */
UENUM()
enum class EVeyraDamageDelivery : uint8
{
	/** An ability's damage, and anything not otherwise named. It does not damage structures. */
	Ability,
	/** A basic attack's hit on its target, empowered or not (§33: structures take damage primarily from basic attacks). */
	BasicAttack,
	/** A structure's own attack, such as a Spire's shot (§55). It is not a basic attack. */
	StructureAttack,
	/** Damage a basic attack or passive spreads to others, such as a cleave or a secondary impact. It does not damage structures. */
	Proc,
	/** A developer command's damage, which reaches structures so tests and smokes can exercise them. */
	Developer,
	/**
	 * A neutral objective drained by the presence of a side's Vanguards (Battleground Bible §6; ADR-014
	 * §4), dealt in the name of one of them so a presence tick can land the last hit. It does not
	 * damage structures.
	 */
	Presence,
	/**
	 * A damage-over-time status's tick, dealt in its source's name (Combat Bible §14; ADR-015 §3), so
	 * rules that treat DoT damage apart, such as Omnivamp's, can find it. It does not damage structures.
	 */
	Periodic,
};

namespace VeyraDamageDelivery
{
	/** Whether damage delivered this way can damage a structure (Combat Bible §33). */
	inline bool DamagesStructures(EVeyraDamageDelivery Delivery)
	{
		return Delivery == EVeyraDamageDelivery::BasicAttack || Delivery == EVeyraDamageDelivery::StructureAttack
			|| Delivery == EVeyraDamageDelivery::Developer;
	}
}

/** One typed component of a damage event. Components resolve independently (Combat Bible §25). */
struct FVeyraDamageComponent
{
	EVeyraDamageType Type = EVeyraDamageType::Physical;
	double Amount = 0.0;
};

/** One damage event can carry at most one component per type. */
using FVeyraDamageComponents = TArray<FVeyraDamageComponent, TInlineAllocator<3>>;

/** An attacker's penetration against one resistance (Combat Bible §3). */
struct FVeyraPenetration
{
	/** Subtracted from positive resistance after percentage penetration. */
	double Flat = 0.0;

	/** The fraction of positive resistance kept by percentage penetration: Π(1 − x) over sources. */
	double Retained = 1.0;
};

/** Step 2 of §25: the raw damage event before any modifier. */
struct FVeyraRawDamageEvent
{
	FVeyraDamageComponents Components;

	/**
	 * Penetration this event carries itself, such as an armour-piercing shot's, on top of its
	 * attacker's: flat values add and retained fractions multiply (§3). The defaults add nothing.
	 */
	FVeyraPenetration PhysicalPenetration;
	FVeyraPenetration MagicPenetration;

	/** How the event is delivered; an ability's by default. */
	EVeyraDamageDelivery Delivery = EVeyraDamageDelivery::Ability;

	/**
	 * Where the projectile that carries it was launched, as a ranged attack's or a skillshot's: cover judges the shot
	 * from there, wherever its shooter has gone since (ADR-037 §4). Unset for damage no projectile carries.
	 */
	TOptional<FVector> ProjectileFrom;
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
