// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Math/Vector.h"
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
