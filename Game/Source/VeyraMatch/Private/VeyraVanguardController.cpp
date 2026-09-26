// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraVanguardController.h"

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

	// A partial path is allowed: an order toward a point the Vanguard cannot fully reach takes it as
	// close as the path allows.
	const EPathFollowingRequestResult::Type Result = MoveToLocation(Walkable.Location, static_cast<float>(Orders.ArrivalTolerance),
		/*bStopOnOverlap*/ false, /*bUsePathfinding*/ true, /*bProjectDestinationToNavigation*/ false, /*bCanStrafe*/ false,
		/*FilterClass*/ nullptr, /*bAllowPartialPath*/ true);
	return Result == EPathFollowingRequestResult::Failed ? EVeyraOrderRejection::Unreachable : EVeyraOrderRejection::None;
}
