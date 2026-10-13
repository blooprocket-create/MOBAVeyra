// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Movement/VeyraMotion.h"

/** What the player's own machine knows of an order as it gives it (ADR-079 §5). */
struct FVeyraOrderPredictionInputs
{
	/** Where the commanded body is drawn now, and its facing. */
	FVector From = FVector::ZeroVector;
	double Yaw = 0.0;
	/** The speed the server last told for it. */
	double Speed = 0.0;
	/** Where the order sends it: a move's or an Attack Move's point, or an attack's target's place. */
	FVector Goal = FVector::ZeroVector;
	/** How far short of Goal it stops: the orders' arrival tolerance, or an attack's reach to its target, centre to centre. */
	double StopShort = 0.0;
	/** An attack: within reach it only turns to its target. */
	bool bFacesWithinReach = false;
	/** Whether it may walk now, as its machine knows: alive, not held by its statuses, moved by nothing else. */
	bool bMayWalk = false;
	/** The server's clock now, and the order's number. */
	double ServerNow = 0.0;
	uint16 OrderNumber = 0;
};

/** The player's own machine's prediction of the orders it gives (ADR-079 §5). Pure. */
namespace VeyraOrderPrediction
{
	/**
	 * The motion the commanded body is drawn by from the moment the order is given until the server answers it: a walk
	 * straight toward the goal, stopping short of it as the server will; or, for an attack within reach, standing turned
	 * to its target. Unset where the body may not walk, or the order gives it nothing to do.
	 */
	VEYRAMATCH_API TOptional<FVeyraMotion> Predict(const FVeyraOrderPredictionInputs& In);
}
