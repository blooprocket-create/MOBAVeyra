// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraVanguardController.h"

#include "GameFramework/Pawn.h"
#include "Movement/VeyraMovementComponent.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Tuning/VeyraMatchTuningSubsystem.h"

AVeyraVanguardController::AVeyraVanguardController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The Vanguard carries its participant's PlayerState, set by the GameMode after possession.
	bWantsPlayerState = false;
}

EVeyraOrderRejection AVeyraVanguardController::MoveToDestination(const FVector& Destination)
{
	if (!GetPawn())
	{
		return EVeyraOrderRejection::NoVanguard;
	}

	const FVeyraOrdersTuning& Orders = UVeyraMatchTuningSubsystem::Get().Orders;
	const UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	FNavLocation Walkable;
	if (!Navigation || !Navigation->ProjectPointToNavigation(Destination, Walkable, FVector(Orders.DestinationProjectionExtent)))
	{
		return EVeyraOrderRejection::Unreachable;
	}

	MoveOrder = Walkable.Location;
	if (IsMovementLocked())
	{
		// Held, not refused: normal orders never override what owns the movement (Combat Bible §9).
		return EVeyraOrderRejection::None;
	}
	return FollowMoveOrder() == EPathFollowingRequestResult::Failed ? EVeyraOrderRejection::Unreachable : EVeyraOrderRejection::None;
}

void AVeyraVanguardController::PawnPendingDestroy(APawn* DestroyedPawn)
{
	if (DestroyedPawn && DestroyedPawn == GetPawn())
	{
		UnPossess();
	}
}

void AVeyraVanguardController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	// A newer order replacing this path, or a lock pausing it, keeps the order; any other end,
	// arriving or failing, finishes it.
	if (!bStoppingForLock && !Result.HasFlag(FPathFollowingResultFlags::NewRequest))
	{
		MoveOrder.Reset();
	}
}

void AVeyraVanguardController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	WatchMovement(InPawn ? Cast<UVeyraMovementComponent>(InPawn->GetMovementComponent()) : nullptr);
}

void AVeyraVanguardController::OnUnPossess()
{
	// A new body starts without the old one's order.
	WatchMovement(nullptr);
	MoveOrder.Reset();
	Super::OnUnPossess();
}

void AVeyraVanguardController::WatchMovement(UVeyraMovementComponent* Movement)
{
	if (UVeyraMovementComponent* Watched = WatchedMovement.Get())
	{
		Watched->OnMovementLockChanged.Remove(MovementLockHandle);
	}
	MovementLockHandle.Reset();
	WatchedMovement = Movement;
	if (Movement)
	{
		MovementLockHandle = Movement->OnMovementLockChanged.AddUObject(this, &AVeyraVanguardController::OnMovementLockChanged);
	}
}

void AVeyraVanguardController::OnMovementLockChanged(bool bLocked)
{
	if (bLocked)
	{
		TGuardValue<bool> StoppingForLock(bStoppingForLock, true);
		StopMovement();
	}
	else if (MoveOrder.IsSet())
	{
		// The unit may have been moved while locked, so it finds a new path from where it is.
		FollowMoveOrder();
	}
}

bool AVeyraVanguardController::IsMovementLocked() const
{
	const UVeyraMovementComponent* Movement = WatchedMovement.Get();
	return Movement && Movement->IsMovementLocked();
}

EPathFollowingRequestResult::Type AVeyraVanguardController::FollowMoveOrder()
{
	// A partial path is allowed: an order toward a point the Vanguard cannot fully reach takes it as
	// close as the path allows.
	const EPathFollowingRequestResult::Type Result = MoveToLocation(MoveOrder.GetValue(),
		static_cast<float>(UVeyraMatchTuningSubsystem::Get().Orders.ArrivalTolerance), /*bStopOnOverlap*/ false, /*bUsePathfinding*/ true,
		/*bProjectDestinationToNavigation*/ false, /*bCanStrafe*/ false, /*FilterClass*/ nullptr, /*bAllowPartialPath*/ true);
	// Already there, or no path: either way the order is finished.
	if (Result != EPathFollowingRequestResult::RequestSuccessful)
	{
		MoveOrder.Reset();
	}
	return Result;
}
