// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "AIController.h"
#include "Delegates/IDelegateInstance.h"
#include "Misc/Optional.h"
#include "UObject/WeakObjectPtr.h"
#include "VeyraMatchTypes.h"

#include "VeyraVanguardController.generated.h"

class UVeyraBasicAttackComponent;
class UVeyraMovementComponent;

/**
 * The server-side controller that moves one Vanguard (ADR-006 §7). Every Vanguard has one, human or
 * AI; players send it orders through their PlayerController. It is kept across deaths, and it stays
 * the pawn's owner, so no client can move the pawn directly. Disconnect autopilot (Match Flow Bible
 * §4) will drive the same controller.
 *
 * While the Vanguard's movement is locked, for example by a Stun, the controller holds its latest
 * move order and carries it out when the lock ends (ADR-009 §2). Attack and attack-move orders chase
 * their target into range and attack it (ADR-009 §5); a move order replaces them.
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
	 * Attacks Target, chasing it into range, until it dies or another order replaces this one. Refused
	 * for a target that is not a living enemy unit, or a Vanguard with no basic attack. The caller has
	 * already checked the match allows the order.
	 */
	EVeyraOrderRejection AttackUnit(AActor& Target);

	/**
	 * Moves toward the walkable point nearest Destination, attacking each enemy that comes within the
	 * basic attack's acquisition radius on the way and then carrying on. It ends at the destination:
	 * there is no idle acquisition (ADR-008 §9).
	 */
	EVeyraOrderRejection AttackMoveTo(const FVector& Destination);

	/** The unit an attack or attack-move order is attacking now, if any. */
	AActor* GetAttackTarget() const { return AttackTarget.Get(); }

	/** The destination of the attack-move order being carried out, if any. */
	const TOptional<FVector>& GetAttackMoveDestination() const { return AttackMoveDestination; }

	virtual void Tick(float DeltaSeconds) override;

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

	/** Where the controller's own path is taking the Vanguard for an attack or attack-move order. */
	enum class EAttackPath : uint8
	{
		None,
		ToTarget,
		ToDestination,
	};

	/** Carries out an attack or attack-move order one step: chase, attack, wait, or walk on. */
	void UpdateAttackOrder();
	void FollowAttackMove();
	void StopForAttack();
	void ClearAttackOrder();
	UVeyraBasicAttackComponent* GetBasicAttack() const;

	/** The nearest living enemy unit within the attack's acquisition radius of the Vanguard's edge. */
	AActor* FindAttackMoveTarget(const UVeyraBasicAttackComponent& Attacks) const;

	/** The walkable destination of the latest move order, until the Vanguard reaches it or it fails. */
	TOptional<FVector> MoveOrder;

	TWeakObjectPtr<AActor> AttackTarget;
	TOptional<FVector> AttackMoveDestination;
	EAttackPath AttackPath = EAttackPath::None;

	TWeakObjectPtr<UVeyraMovementComponent> WatchedMovement;
	FDelegateHandle MovementLockHandle;

	/** Set while the controller itself stops the path for a lock, so the order is kept. */
	bool bStoppingForLock = false;
};
