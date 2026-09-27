// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "AIController.h"
#include "Delegates/IDelegateInstance.h"
#include "Misc/Optional.h"
#include "UObject/WeakObjectPtr.h"
#include "VeyraMatchTypes.h"

#include "VeyraVanguardController.generated.h"

class UVeyraMovementComponent;

/**
 * The server-side controller that moves one Vanguard (ADR-006 §7). Every Vanguard has one, human or
 * AI; players send it orders through their PlayerController. It is kept across deaths, and it stays
 * the pawn's owner, so no client can move the pawn directly. Disconnect autopilot (Match Flow Bible
 * §4) will drive the same controller.
 *
 * While the Vanguard's movement is locked, for example by a Stun, the controller holds its latest
 * move order and carries it out when the lock ends (ADR-009 §2).
 */
UCLASS(Transient)
class VEYRAMATCH_API AVeyraVanguardController : public AAIController
{
	GENERATED_BODY()

public:
	AVeyraVanguardController(const FObjectInitializer& ObjectInitializer);

	/**
	 * Moves the Vanguard to the walkable point nearest Destination, within the tuned projection
	 * extent, by pathfinding; while its movement is locked, holds the order until the lock ends. The
	 * caller has already checked the match allows the order.
	 */
	EVeyraOrderRejection MoveToDestination(const FVector& Destination);

	/** The move order being carried out or held, if the Vanguard has not reached it yet. */
	const TOptional<FVector>& GetMoveOrder() const { return MoveOrder; }

	/**
	 * Lets go of a body leaving the map but stays alive to possess the next one. The engine would
	 * destroy a controller without a PlayerState here; this one belongs to its participant.
	 */
	virtual void PawnPendingDestroy(APawn* DestroyedPawn) override;

	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

private:
	void WatchMovement(UVeyraMovementComponent* Movement);
	void OnMovementLockChanged(bool bLocked);
	bool IsMovementLocked() const;

	/** Starts walking to the current move order, finishing it if it is already reached or has no path. */
	EPathFollowingRequestResult::Type FollowMoveOrder();

	/** The walkable destination of the latest move order, until the Vanguard reaches it or it fails. */
	TOptional<FVector> MoveOrder;

	TWeakObjectPtr<UVeyraMovementComponent> WatchedMovement;
	FDelegateHandle MovementLockHandle;

	/** Set while the controller itself stops the path for a lock, so the order is kept. */
	bool bStoppingForLock = false;
};
