// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Fluxborn/VeyraFluxbornController.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attacks/VeyraBasicAttackComponent.h"
#include "Engine/World.h"
#include "Fluxborn/VeyraFluxborn.h"
#include "Movement/VeyraMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Shapes/VeyraShapes.h"
#include "Targeting/VeyraTargeting.h"
#include "Terrain/VeyraSurfacePlacement.h"
#include "TimerManager.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"
#include "VeyraCombatVerbs.h"

AVeyraFluxbornController::AVeyraFluxbornController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The server moves every Fluxborn; clients never see their controllers.
	bReplicates = false;
	bSetControlRotationFromPawnOrientation = false;
}

void AVeyraFluxbornController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	// It finds its place on the lane from where it stands.
	WaypointIndex = 1;
	bOffLane = true;
	if (UVeyraMovementComponent* Movement = InPawn ? Cast<UVeyraMovementComponent>(InPawn->GetMovementComponent()) : nullptr)
	{
		WatchedMovement = Movement;
		MovementLockHandle = Movement->OnMovementLockChanged.AddUObject(this, &AVeyraFluxbornController::OnMovementLockChanged);
	}
	// On world time, so a pause holds it (ADR-006 §8).
	const float Cadence = static_cast<float>(UVeyraWorldTuningSubsystem::Get().Fluxborn.Ai.ThinkSeconds);
	GetWorldTimerManager().SetTimer(ThinkTimer, FTimerDelegate::CreateUObject(this, &AVeyraFluxbornController::Think), Cadence, /*bLoop*/ true);
}

void AVeyraFluxbornController::OnUnPossess()
{
	GetWorldTimerManager().ClearTimer(ThinkTimer);
	if (UVeyraMovementComponent* Movement = WatchedMovement.Get())
	{
		Movement->OnMovementLockChanged.Remove(MovementLockHandle);
	}
	WatchedMovement.Reset();
	Target.Reset();
	Claimant.Reset();
	Super::OnUnPossess();
}

void AVeyraFluxbornController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ThinkTimer);
	Super::EndPlay(EndPlayReason);
}

void AVeyraFluxbornController::NoteAggression(AActor& Attacker)
{
	Claimant = &Attacker;
}

void AVeyraFluxbornController::Think()
{
	AVeyraFluxborn* Body = GetFluxborn();
	if (!Body || !Body->IsAlive())
	{
		GetWorldTimerManager().ClearTimer(ThinkTimer);
		return;
	}
	UVeyraBasicAttackComponent* Attacks = Body->GetBasicAttack();
	const UVeyraMovementComponent* Movement = WatchedMovement.Get();
	// It holds while something owns its movement, and lets an attack's windup finish.
	if (!Attacks || (Movement && Movement->IsMovementLocked()) || Attacks->GetState().Phase == EVeyraAttackPhase::Windup)
	{
		return;
	}

	// Strayed beyond its leash, by a chase or a push, it lets its target go and walks back to its lane, however near the
	// lane the target stands: a call for help cannot hold it out there (ADR-065 §2).
	if (VeyraFluxbornRules::DistanceFromLane(Body->GetWaypoints(), FVector2D(Body->GetActorLocation())) > UVeyraWorldTuningSubsystem::Get().Fluxborn.Ai.LeashRange)
	{
		Claimant.Reset();
		Target.Reset();
		bResponding = false;
		Walk(*Body);
		return;
	}

	const FVeyraFluxbornDefinition* Definition = Body->GetDefinition();
	const bool bSiege = Definition && Definition->Role == EVeyraFluxbornRole::Siege;
	const FVeyraFluxbornChoice Choice = VeyraFluxbornRules::Choose(Target.Get(), bResponding, Claimant.Get(), bSiege, GatherCandidates());
	Claimant.Reset();
	Target = const_cast<AActor*>(Choice.Target);
	bResponding = Choice.bResponding;
	if (AActor* Enemy = Target.Get())
	{
		bOffLane = true;
		Engage(*Enemy, *Attacks);
	}
	else
	{
		Walk(*Body);
	}
}

TArray<FVeyraFluxbornCandidate> AVeyraFluxbornController::GatherCandidates() const
{
	TArray<FVeyraFluxbornCandidate> Candidates;
	const AVeyraFluxborn* Body = GetFluxborn();
	const UVeyraBasicAttackComponent* Attacks = Body ? Body->GetBasicAttack() : nullptr;
	if (!Attacks || !Attacks->HasProfile())
	{
		return Candidates;
	}
	const FVeyraBasicAttackProfile& Profile = Attacks->GetProfile();
	const double Leash = UVeyraWorldTuningSubsystem::Get().Fluxborn.Ai.LeashRange;
	const TArray<FVector2D>& Lane = Body->GetWaypoints();
	// A circle this wide around its centre touches every body within its acquisition radius of its edge.
	FVeyraShape Reach;
	Reach.Kind = EVeyraShapeKind::Circle;
	Reach.Radius = Profile.AcquisitionRadius + Body->GetSimpleCollisionRadius();
	const TArray<AActor*> Units = VeyraShapes::GatherUnits(*GetWorld(), FVeyraPlacedShape{ Reach, Body->GetActorLocation(), Body->GetActorForwardVector() },
		[Body, &Lane, Leash](const AActor& Unit) {
			if (!VeyraTargeting::AreHostile(Body, &Unit) || !VeyraTargeting::CanAcquire(Body, Unit)
				|| VeyraFluxbornRules::DistanceFromLane(Lane, FVector2D(Unit.GetActorLocation())) > Leash)
			{
				return false;
			}
			// A structure its prerequisites protect cannot be damaged, so it is walked past (Battleground Bible §18).
			const UAbilitySystemComponent* Structure = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit);
			return !VeyraUnits::IsStructure(&Unit) || (Structure && !VeyraCombat::IsInvulnerable(*Structure));
		},
		EVeyraStructureTargeting::Allow);
	for (const AActor* Unit : Units)
	{
		const double Distance = VeyraTargeting::EdgeToEdgeDistance(*Body, *Unit);
		Candidates.Add({ Unit, VeyraUnits::KindOf(Unit).Get(EVeyraUnitKind::Fluxborn), Distance, Distance <= Profile.Range, Unit->GetUniqueID() });
	}
	return Candidates;
}

void AVeyraFluxbornController::Engage(AActor& Enemy, UVeyraBasicAttackComponent& Attacks)
{
	switch (Attacks.CheckAttack(&Enemy))
	{
	case EVeyraAttackRejection::None:
		Halt();
		Attacks.StartAttack(Enemy);
		break;
	case EVeyraAttackRejection::OutOfRange:
		if (Path != EPath::Chase || ChaseTarget.Get() != &Enemy)
		{
			// Closing cuts a backswing short; the path follows the enemy and a later thought stops it in range.
			Attacks.CancelAttack();
			const EPathFollowingRequestResult::Type Result = MoveToActor(&Enemy, /*AcceptanceRadius*/ 0.0f, /*bStopOnOverlap*/ true,
				/*bUsePathfinding*/ true, /*bCanStrafe*/ false, /*FilterClass*/ nullptr, /*bAllowPartialPath*/ true);
			Path = Result == EPathFollowingRequestResult::RequestSuccessful ? EPath::Chase : EPath::None;
			ChaseTarget = &Enemy;
		}
		break;
	case EVeyraAttackRejection::OnCooldown:
		// In range: it waits there for its attack's interval.
		Halt();
		break;
	case EVeyraAttackRejection::InvalidTarget:
		Target.Reset();
		bResponding = false;
		break;
	default:
		break;
	}
}

void AVeyraFluxbornController::Walk(const AVeyraFluxborn& Body)
{
	const TArray<FVector2D>& Waypoints = Body.GetWaypoints();
	if (Waypoints.IsEmpty())
	{
		return;
	}
	const FVector Here = Body.GetActorLocation();
	const FVector2D Where(Here);
	// After a fight it rejoins at the first waypoint ahead, never behind (ADR-011 §7).
	if (bOffLane)
	{
		bOffLane = false;
		WaypointIndex = VeyraFluxbornRules::ResumeWaypoint(Waypoints, Where);
		Path = EPath::None;
	}
	WaypointIndex = FMath::Clamp(WaypointIndex, 0, Waypoints.Num() - 1);
	const double Acceptance = UVeyraWorldTuningSubsystem::Get().Fluxborn.Ai.WaypointAcceptance;
	bool bArrived = FVector2D::Distance(Where, Waypoints[WaypointIndex]) <= Acceptance;
	if (!bArrived && Path == EPath::Lane && GetPathFollowingComponent()->GetStatus() != EPathFollowingStatus::Idle)
	{
		return;
	}
	if (!bArrived)
	{
		FVector Destination;
		if (!VeyraSurfacePlacement::Resolve(*GetWorld(), Waypoints[WaypointIndex], 0.0, UVeyraWorldTuningSubsystem::Get().Layout.Surface, Destination))
		{
			Halt();
			return;
		}
		const EPathFollowingRequestResult::Type Result = MoveToLocation(Destination, static_cast<float>(Acceptance),
			/*bStopOnOverlap*/ false, /*bUsePathfinding*/ true, /*bProjectDestinationToNavigation*/ true, /*bCanStrafe*/ false, /*FilterClass*/ nullptr,
			/*bAllowPartialPath*/ true);
		Path = Result == EPathFollowingRequestResult::RequestSuccessful ? EPath::Lane : EPath::None;
		// The path follower may judge it there when this measure does not.
		bArrived = Result == EPathFollowingRequestResult::AlreadyAtGoal;
	}
	if (bArrived)
	{
		Path = EPath::None;
		// At the last waypoint, the enemy base's heart, it waits for something to attack.
		WaypointIndex = FMath::Min(WaypointIndex + 1, Waypoints.Num() - 1);
	}
}

void AVeyraFluxbornController::Halt()
{
	if (Path != EPath::None || GetPathFollowingComponent()->GetStatus() != EPathFollowingStatus::Idle)
	{
		Path = EPath::None;
		ChaseTarget.Reset();
		StopMovement();
	}
}

void AVeyraFluxbornController::OnMovementLockChanged(bool bLocked)
{
	if (bLocked)
	{
		// Its next thought after the lock finds a path from wherever it was moved to.
		Halt();
		bOffLane = true;
	}
}

AVeyraFluxborn* AVeyraFluxbornController::GetFluxborn() const
{
	return Cast<AVeyraFluxborn>(GetPawn());
}
