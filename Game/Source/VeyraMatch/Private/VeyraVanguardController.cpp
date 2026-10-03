// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraVanguardController.h"

#include "Algo/MinElement.h"

#include "Attacks/VeyraBasicAttackComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Movement/VeyraMovementComponent.h"
#include "Movement/VeyraUnitCollision.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Shapes/VeyraShapes.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraMatchTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraMatchLog.h"

namespace
{
	/** The walkable point nearest Destination within the tuned projection extent, if there is one. */
	TOptional<FVector> ProjectOrderDestination(const UWorld* World, const FVector& Destination)
	{
		const FVeyraOrdersTuning& Orders = UVeyraMatchTuningSubsystem::Get().Orders;
		const UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		FNavLocation Walkable;
		if (!Navigation || !Navigation->ProjectPointToNavigation(Destination, Walkable, FVector(Orders.DestinationProjectionExtent)))
		{
			return {};
		}
		return Walkable.Location;
	}
}

AVeyraVanguardController::AVeyraVanguardController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The Vanguard carries its participant's PlayerState, set by the GameMode after possession.
	bWantsPlayerState = false;
	// Attack orders follow their target every tick.
	PrimaryActorTick.bCanEverTick = true;
}

EVeyraOrderRejection AVeyraVanguardController::MoveToDestination(const FVector& Destination)
{
	if (!GetPawn())
	{
		return EVeyraOrderRejection::NoVanguard;
	}
	const TOptional<FVector> Walkable = ProjectOrderDestination(GetWorld(), Destination);
	if (!Walkable.IsSet())
	{
		return EVeyraOrderRejection::Unreachable;
	}

	// Moving replaces an attack order, cancels an attack before its Commit and cuts a backswing short
	// (§48); a mobile attacker's windup goes on while it walks (ADR-027 §1).
	ClearAttackOrder();
	if (UVeyraBasicAttackComponent* Attacks = GetBasicAttack(); Attacks && !IsMobileWindup(*Attacks))
	{
		Attacks->CancelAttack();
	}
	MoveOrder = Walkable.GetValue();
	if (IsMovementLocked())
	{
		// Held, not refused: normal orders never override what owns the movement (Combat Bible §9).
		return EVeyraOrderRejection::None;
	}
	return FollowMoveOrder() == EPathFollowingRequestResult::Failed ? EVeyraOrderRejection::Unreachable : EVeyraOrderRejection::None;
}

EVeyraOrderRejection AVeyraVanguardController::AttackUnit(AActor& Target)
{
	const APawn* Body = GetPawn();
	if (!Body)
	{
		return EVeyraOrderRejection::NoVanguard;
	}
	UVeyraBasicAttackComponent* Attacks = GetBasicAttack();
	const bool bEnemyUnit = VeyraUnits::KindOf(&Target).IsSet() && VeyraTargeting::IsAlive(&Target) && VeyraTargeting::AreHostile(Body, &Target);
	if (!Attacks || !Attacks->HasProfile() || !bEnemyUnit)
	{
		return EVeyraOrderRejection::CannotAttack;
	}
	// An order for a unit the player's side cannot see, such as one sent just as it slipped into fog,
	// is refused: nobody may target what they cannot see (Vision Bible §1).
	if (!VeyraTargeting::CanAcquire(Body, Target))
	{
		return EVeyraOrderRejection::CannotAttack;
	}

	// A new target takes over from an attack still winding up; the same one again changes nothing.
	if (AttackTarget.Get() != &Target && Attacks->GetState().Phase == EVeyraAttackPhase::Windup)
	{
		Attacks->CancelAttack();
	}
	MoveOrder.Reset();
	AttackMoveDestination.Reset();
	AttackMoveAim.Reset();
	AttackTarget = &Target;
	AttackPath = EAttackPath::None;
	TracedAnswer.Reset();
	UE_LOG(LogVeyraMatch, Verbose, TEXT("%s takes an attack order on %s."), *GetNameSafe(Body), *GetNameSafe(&Target));
	UpdateAttackOrder();
	return EVeyraOrderRejection::None;
}

EVeyraOrderRejection AVeyraVanguardController::AttackMoveTo(const FVector& Destination, EVeyraAttackMoveTarget Preference)
{
	if (!GetPawn())
	{
		return EVeyraOrderRejection::NoVanguard;
	}
	// A rider cannot attack, so its attack-move is an ordinary move (Combat Bible §56).
	if (const UVeyraMovementComponent* Movement = GetPawn()->FindComponentByClass<UVeyraMovementComponent>(); Movement && Movement->IsRiding())
	{
		return MoveToDestination(Destination);
	}
	UVeyraBasicAttackComponent* Attacks = GetBasicAttack();
	if (!Attacks || !Attacks->HasProfile())
	{
		return EVeyraOrderRejection::CannotAttack;
	}
	const TOptional<FVector> Walkable = ProjectOrderDestination(GetWorld(), Destination);
	if (!Walkable.IsSet())
	{
		return EVeyraOrderRejection::Unreachable;
	}

	if (Attacks->GetState().Phase == EVeyraAttackPhase::Windup && !IsMobileWindup(*Attacks))
	{
		Attacks->CancelAttack();
	}
	MoveOrder.Reset();
	AttackTarget.Reset();
	AttackMoveDestination = Walkable.GetValue();
	AttackMoveAim.Reset();
	if (Preference == EVeyraAttackMoveTarget::ClosestToCursor)
	{
		AttackMoveAim = Destination;
	}
	AttackPath = EAttackPath::None;
	// A mobile attacker walks on while its windup goes on, as it does for a move order (ADR-027 §1); the
	// order takes up its next target once the attack commits.
	if (Attacks->GetState().Phase == EVeyraAttackPhase::Windup && !IsMovementLocked())
	{
		FollowAttackMove();
	}
	UpdateAttackOrder();
	return EVeyraOrderRejection::None;
}

void AVeyraVanguardController::StopOrders()
{
	MoveOrder.Reset();
	ClearAttackOrder();
	if (UVeyraBasicAttackComponent* Attacks = GetBasicAttack(); Attacks && Attacks->GetState().Phase == EVeyraAttackPhase::Windup)
	{
		Attacks->CancelAttack();
	}
	StopMovement();
}

void AVeyraVanguardController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		UpdateAttackOrder();
		UpdateRideArrival();
		UpdateOccupiedArrival();
	}
}

void AVeyraVanguardController::UpdateOccupiedArrival()
{
	// A destination an enemy or a neutral unit stands on can't be reached: the Vanguard would steer round that body
	// without end. It arrives instead on reaching the body's edge (ADR-062 §3).
	const APawn* Body = GetPawn();
	const UPrimitiveComponent* Capsule = Body ? Cast<UPrimitiveComponent>(Body->GetRootComponent()) : nullptr;
	if (!MoveOrder.IsSet() || !Capsule || GetPathFollowingComponent()->GetStatus() != EPathFollowingStatus::Moving)
	{
		return;
	}
	const double Radius = Body->GetSimpleCollisionRadius();
	const double Tolerance = UVeyraMatchTuningSubsystem::Get().Orders.ArrivalTolerance;
	TArray<FOverlapResult> Occupants;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(VeyraOccupiedArrival), /*bTraceComplex*/ false, Body);
	GetWorld()->OverlapMultiByObjectType(Occupants, MoveOrder.GetValue(), FQuat::Identity, VeyraUnitCollision::AllUnits(), FCollisionShape::MakeSphere(Radius), Query);
	for (const FOverlapResult& Occupant : Occupants)
	{
		const UPrimitiveComponent* Other = Occupant.GetComponent();
		const AActor* Unit = Occupant.GetActor();
		if (!Other || !Unit || Capsule->GetCollisionResponseToChannel(Other->GetCollisionObjectType()) != ECR_Block)
		{
			continue;
		}
		const double Gap = FVector::Dist2D(Body->GetActorLocation(), Unit->GetActorLocation()) - Radius - Unit->GetSimpleCollisionRadius();
		if (Gap <= Tolerance)
		{
			MoveOrder.Reset();
			StopMovement();
			return;
		}
	}
}

void AVeyraVanguardController::UpdateRideArrival()
{
	// A rider cannot pivot, so a destination inside its turning circle is reached at its closest
	// approach: once within that circle it stops as soon as it starts moving away (ADR-018 §8).
	const APawn* Body = GetPawn();
	const UVeyraMovementComponent* Movement = Body ? Body->FindComponentByClass<UVeyraMovementComponent>() : nullptr;
	if (!MoveOrder.IsSet() || !Movement || !Movement->IsRiding())
	{
		RideClosest.Reset();
		return;
	}
	const double Distance = FVector::Dist2D(Body->GetActorLocation(), MoveOrder.GetValue());
	if (Distance > Movement->GetRideTurnRadius())
	{
		RideClosest.Reset();
		return;
	}
	if (RideClosest.IsSet() && Distance > RideClosest.GetValue())
	{
		RideClosest.Reset();
		StopOrders();
		return;
	}
	RideClosest = Distance;
}

void AVeyraVanguardController::UpdateAttackOrder()
{
	if (!AttackTarget.IsValid() && !AttackMoveDestination.IsSet())
	{
		return;
	}
	const APawn* Body = GetPawn();
	UVeyraBasicAttackComponent* Attacks = GetBasicAttack();
	if (!Body || !Attacks)
	{
		DropAttackOrder(TEXT("no body, or no basic attack"));
		return;
	}
	// The order waits while something owns the movement, and while an attack winds up.
	if (IsMovementLocked() || Attacks->GetState().Phase == EVeyraAttackPhase::Windup)
	{
		return;
	}

	AActor* Target = AttackTarget.Get();
	if (Target && (!VeyraTargeting::IsAlive(Target) || !VeyraTargeting::AreHostile(Body, Target)))
	{
		Target = nullptr;
		AttackTarget.Reset();
	}
	if (AttackMoveDestination.IsSet())
	{
		// Attack-move lets go of an enemy that leaves the acquisition radius, and takes the nearest one in it.
		if (Target && VeyraTargeting::EdgeToEdgeDistance(*Body, *Target) > Attacks->GetProfile().AcquisitionRadius)
		{
			Target = nullptr;
			AttackTarget.Reset();
		}
		if (!Target)
		{
			Target = FindAttackMoveTarget(*Attacks);
			AttackTarget = Target;
			if (Target)
			{
				// Closest to Cursor chooses the order's first enemy only (ADR-041 §8.5).
				AttackMoveAim.Reset();
			}
		}
		if (!Target)
		{
			if (AttackPath != EAttackPath::ToDestination)
			{
				FollowAttackMove();
			}
			return;
		}
	}
	else if (!Target)
	{
		// The unit it attacked died or became invalid. The Vanguard stands: there is no idle acquisition.
		DropAttackOrder(TEXT("its target died or is no longer an enemy"));
		return;
	}

	const EVeyraAttackRejection Answer = Attacks->CheckAttack(Target);
	TraceAttack(*Target, Answer);
	switch (Answer)
	{
	case EVeyraAttackRejection::None:
		StopForAttack();
		Attacks->StartAttack(*Target);
		break;
	case EVeyraAttackRejection::OutOfRange:
		if (AttackPath != EAttackPath::ToTarget)
		{
			// Chasing cuts a backswing short. The path follows the target; the next steps stop it in range.
			Attacks->CancelAttack();
			const EPathFollowingRequestResult::Type Result = MoveToActor(Target, /*AcceptanceRadius*/ 0.0f, /*bStopOnOverlap*/ true,
				/*bUsePathfinding*/ true, /*bCanStrafe*/ false, /*FilterClass*/ nullptr, /*bAllowPartialPath*/ true);
			AttackPath = Result == EPathFollowingRequestResult::RequestSuccessful ? EAttackPath::ToTarget : EAttackPath::None;
		}
		break;
	case EVeyraAttackRejection::OnCooldown:
		// In range: it waits there for the attack's interval.
		StopForAttack();
		break;
	case EVeyraAttackRejection::InvalidTarget:
	// A target that slips out of sight is dropped: nobody may target what they cannot see (Vision Bible §1).
	case EVeyraAttackRejection::NotVisible:
		AttackTarget.Reset();
		if (!AttackMoveDestination.IsSet())
		{
			DropAttackOrder(LexToString(Answer));
		}
		break;
	case EVeyraAttackRejection::NoProfile:
	case EVeyraAttackRejection::AttackerDead:
		DropAttackOrder(LexToString(Answer));
		break;
	case EVeyraAttackRejection::CrowdControlled:
	case EVeyraAttackRejection::Busy:
		break;
	}
}

void AVeyraVanguardController::FollowAttackMove()
{
	const EPathFollowingRequestResult::Type Result = MoveToLocation(AttackMoveDestination.GetValue(),
		static_cast<float>(UVeyraMatchTuningSubsystem::Get().Orders.ArrivalTolerance), /*bStopOnOverlap*/ false, /*bUsePathfinding*/ true,
		/*bProjectDestinationToNavigation*/ false, /*bCanStrafe*/ false, /*FilterClass*/ nullptr, /*bAllowPartialPath*/ true);
	if (Result == EPathFollowingRequestResult::RequestSuccessful)
	{
		AttackPath = EAttackPath::ToDestination;
	}
	else
	{
		// Already there, or no path: the attack-move is finished.
		ClearAttackOrder();
	}
}

bool AVeyraVanguardController::IsMobileWindup(const UVeyraBasicAttackComponent& Attacks)
{
	return Attacks.GetState().Phase == EVeyraAttackPhase::Windup && Attacks.GetWindupMovementShare() > 0.0;
}

void AVeyraVanguardController::StopForAttack()
{
	if (AttackPath != EAttackPath::None || GetPathFollowingComponent()->GetStatus() != EPathFollowingStatus::Idle)
	{
		AttackPath = EAttackPath::None;
		StopMovement();
	}
}

void AVeyraVanguardController::ClearAttackOrder()
{
	AttackTarget.Reset();
	AttackMoveDestination.Reset();
	AttackMoveAim.Reset();
	AttackPath = EAttackPath::None;
	TracedAnswer.Reset();
}

void AVeyraVanguardController::DropAttackOrder(const TCHAR* Why)
{
	UE_LOG(LogVeyraMatch, Verbose, TEXT("%s drops its attack order on %s: %s."), *GetNameSafe(GetPawn()), *GetNameSafe(AttackTarget.Get()), Why);
	ClearAttackOrder();
}

void AVeyraVanguardController::TraceAttack(const AActor& Target, EVeyraAttackRejection Answer)
{
	if (TracedAnswer.IsSet() && TracedAnswer.GetValue() == Answer)
	{
		return;
	}
	TracedAnswer = Answer;
	UE_LOG(LogVeyraMatch, Verbose, TEXT("%s's attack on %s: %s."), *GetNameSafe(GetPawn()), *GetNameSafe(&Target), LexToString(Answer));
}

UVeyraBasicAttackComponent* AVeyraVanguardController::GetBasicAttack() const
{
	// A Vanguard's attack is its participant's; a unit it drives without one, as an Echo, carries its own (ADR-050 §6).
	const APawn* Body = GetPawn();
	const APlayerState* Participant = Body ? Body->GetPlayerState() : nullptr;
	return Participant ? Participant->FindComponentByClass<UVeyraBasicAttackComponent>() : Body ? Body->FindComponentByClass<UVeyraBasicAttackComponent>() : nullptr;
}

AActor* AVeyraVanguardController::FindAttackMoveTarget(const UVeyraBasicAttackComponent& Attacks) const
{
	const APawn* Body = GetPawn();
	// A circle this wide around the Vanguard's centre touches every body within the radius of its edge.
	FVeyraShape Reach;
	Reach.Kind = EVeyraShapeKind::Circle;
	Reach.Radius = Attacks.GetProfile().AcquisitionRadius + Body->GetSimpleCollisionRadius();
	// Like any basic attack, an attack-move may pick a structure (Combat Bible §33).
	const TArray<AActor*> Enemies = VeyraShapes::GatherUnits(*GetWorld(), FVeyraPlacedShape{ Reach, Body->GetActorLocation(), Body->GetActorForwardVector() },
		[Body](const AActor& Unit) { return VeyraTargeting::AreHostile(Body, &Unit) && VeyraTargeting::CanAcquire(Body, Unit); }, EVeyraStructureTargeting::Allow);
	if (Enemies.IsEmpty())
	{
		return nullptr;
	}
	if (!AttackMoveAim.IsSet())
	{
		// Nearest the Vanguard: GatherUnits gives them nearest first.
		return Enemies[0];
	}
	// Closest to Cursor: of those in reach, the one nearest the point the order was given at.
	const FVector Aim = AttackMoveAim.GetValue();
	AActor* const* Nearest = Algo::MinElementBy(Enemies, [&Aim](const AActor* Unit) { return FVector::DistSquared2D(Unit->GetActorLocation(), Aim); });
	return Nearest ? *Nearest : nullptr;
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
		// Reaching an attack-move's destination with nothing to attack finishes it too.
		if (AttackPath == EAttackPath::ToDestination && Result.IsSuccess() && !AttackTarget.IsValid())
		{
			ClearAttackOrder();
		}
	}
	// The attack order finds its next path on its next step.
	if (!Result.HasFlag(FPathFollowingResultFlags::NewRequest))
	{
		AttackPath = EAttackPath::None;
	}
}

void AVeyraVanguardController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	WatchMovement(InPawn ? Cast<UVeyraMovementComponent>(InPawn->GetMovementComponent()) : nullptr);
}

void AVeyraVanguardController::OnUnPossess()
{
	// A new body starts without the old one's orders.
	WatchMovement(nullptr);
	MoveOrder.Reset();
	ClearAttackOrder();
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
