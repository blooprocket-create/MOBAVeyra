// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "AIController.h"
#include "VeyraMatchTypes.h"

#include "VeyraVanguardController.generated.h"

/**
 * The server-side controller that moves one Vanguard (ADR-006 §7). Every Vanguard has one, human or
 * AI; players send it orders through their PlayerController. It is kept across deaths, and it stays
 * the pawn's owner, so no client can move the pawn directly. Disconnect autopilot (Match Flow Bible
 * §4) will drive the same controller.
 */
UCLASS(Transient)
class VEYRAMATCH_API AVeyraVanguardController : public AAIController
{
	GENERATED_BODY()

public:
	AVeyraVanguardController(const FObjectInitializer& ObjectInitializer);

	/**
	 * Moves the Vanguard to the walkable point nearest Destination, within the tuned projection
	 * extent, by pathfinding. The caller has already checked the match allows the order.
	 */
	EVeyraOrderRejection MoveToDestination(const FVector& Destination);

	/**
	 * Lets go of a body leaving the map but stays alive to possess the next one. The engine would
	 * destroy a controller without a PlayerState here; this one belongs to its participant.
	 */
	virtual void PawnPendingDestroy(APawn* DestroyedPawn) override;
};
