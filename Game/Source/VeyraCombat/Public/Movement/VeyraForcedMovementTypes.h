// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Math/Vector.h"
#include "Statuses/VeyraStatusTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/WeakObjectPtr.h"

#include "VeyraForcedMovementTypes.generated.h"

class AActor;

/** Whether a dash stops when it reaches an enemy unit (Combat Bible §9). */
UENUM()
enum class EVeyraDashContact : uint8
{
	/** It runs its whole path through units. */
	None,
	/** It stops touching the first living enemy unit in its path, as a charge does. */
	StopAtFirstEnemy,
};

/**
 * One displacement of a unit by another source, a Knockback or a Pull (Combat Bible §8, §9), with
 * its values worked out by the caller from data. Displacement Resistance shortens Distance.
 */
struct FVeyraDisplacement
{
	/** Along the ground; only its horizontal part counts. */
	FVector Direction = FVector::ZeroVector;

	/** Units, before Displacement Resistance; above 0. */
	double Distance = 0.0;

	/** Units per second; above 0. */
	double Speed = 0.0;

	/**
	 * Given the unit, from the displacing source, if the displacement collides (ADR-028 §3): terrain
	 * shortens its path, or its body meets another Vanguard or a structure on the way, where it stops.
	 * Empty for one that collides with nothing.
	 */
	TArray<FVeyraStatusSpec> CollisionStatuses;
};

/** One dash: a unit's own movement (Combat Bible §9). Terrain stops it; it never crosses terrain. */
struct FVeyraDash
{
	FVector Direction = FVector::ZeroVector;
	double Distance = 0.0;
	double Speed = 0.0;
	EVeyraDashContact Contact = EVeyraDashContact::None;
};

/** Why a dash ended. */
enum class EVeyraDashEndReason : uint8
{
	/** It reached the end of its path. */
	Arrived,
	/** It stopped at an enemy unit. */
	EnemyContact,
	/** A displacement took over the unit's movement (§9). */
	Interrupted,
};

struct FVeyraDashEnd
{
	EVeyraDashEndReason Reason = EVeyraDashEndReason::Arrived;

	/** The enemy it stopped at, for EnemyContact. */
	TWeakObjectPtr<AActor> Contact;
};

/** A ride state's movement (Combat Bible §56), with its values worked out from data. */
struct FVeyraRide
{
	/** The rider's Movement Speed, set rather than added; above 0. */
	double SetSpeed = 0.0;

	/** How fast its heading may turn, in degrees per second, whatever its speed; above 0. */
	double TurnRateDegreesPerSecond = 0.0;

	/** Seconds over which it slows back to its ordinary speed once the ride ends; at least 0. */
	double DecaySeconds = 0.0;
};

/** Why a ride ended (Combat Bible §56, "Leaving"). */
enum class EVeyraRideEndReason : uint8
{
	/** Its rider left it: a dismount, or a recast that separates rider and vehicle. */
	Dismounted,
	/** Its time ran out. */
	Expired,
	/** Its rider died. */
	Died,
};

struct FVeyraRideEnd
{
	EVeyraRideEndReason Reason = EVeyraRideEndReason::Expired;

	/** Where the rider was and which way it was heading as it ended, for its vehicle. */
	FVector Location = FVector::ZeroVector;
	FVector Heading = FVector::ForwardVector;
};

/** Why a unit let go of the body it held on to (ADR-018 §2). */
enum class EVeyraAttachEndReason : uint8
{
	/** Its time ran out. */
	Expired,
	/** It let go itself, as a recast does. */
	Released,
	/** The host died or is gone. */
	HostLost,
	/** The unit itself died. */
	Died,
	/** A displacement, a Fear or another attach took over its movement. */
	Replaced,
};

struct FVeyraAttachEnd
{
	EVeyraAttachEndReason Reason = EVeyraAttachEndReason::Expired;

	/** The body it held on to. */
	TWeakObjectPtr<AActor> Host;
};
