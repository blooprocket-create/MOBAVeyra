// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Companions/VeyraCompanionController.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attacks/VeyraBasicAttackComponent.h"
#include "Attribution/VeyraAttributionComponent.h"
#include "Companions/VeyraCompanion.h"
#include "Companions/VeyraCompanionRules.h"
#include "Engine/World.h"
#include "Statuses/VeyraStatusTypes.h"
#include "Movement/VeyraMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Shapes/VeyraShapes.h"
#include "Targeting/VeyraTargeting.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraCombatVerbs.h"

namespace
{
	/** Whether Owner contributed against Unit no longer than Seconds before Now (Combat Bible §18's records). */
	bool OwnerFoughtLately(const AActor& Unit, const UAbilitySystemComponent& Owner, double Now, double Seconds)
	{
		const UAbilitySystemComponent* Abilities = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit);
		const AActor* Keeper = Abilities ? Abilities->GetOwner() : nullptr;
		const UVeyraAttributionComponent* Attribution = Keeper ? Keeper->FindComponentByClass<UVeyraAttributionComponent>() : nullptr;
		return Attribution && Attribution->GetContributions().ContainsByPredicate([&Owner, Now, Seconds](const FVeyraContribution& Each) {
			return Each.Contributor.Get() == &Owner && Now - Each.AtSeconds <= Seconds;
		});
	}
}

AVeyraCompanionController::AVeyraCompanionController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The server moves every companion; clients never see their controllers.
	bReplicates = false;
	bSetControlRotationFromPawnOrientation = false;
}

void AVeyraCompanionController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (UVeyraMovementComponent* Movement = InPawn ? Cast<UVeyraMovementComponent>(InPawn->GetMovementComponent()) : nullptr)
	{
		WatchedMovement = Movement;
		MovementLockHandle = Movement->OnMovementLockChanged.AddUObject(this, &AVeyraCompanionController::OnMovementLockChanged);
	}
	// On world time, so a pause holds it (ADR-006 §8).
	const AVeyraCompanion* Body = Cast<AVeyraCompanion>(InPawn);
	const FVeyraCompanionTuning* Tuning = Body ? Body->GetDefinition() : nullptr;
	if (Tuning)
	{
		GetWorldTimerManager().SetTimer(ThinkTimer, FTimerDelegate::CreateUObject(this, &AVeyraCompanionController::Think),
			static_cast<float>(Tuning->ThinkSeconds), /*bLoop*/ true);
	}
}

void AVeyraCompanionController::OnUnPossess()
{
	GetWorldTimerManager().ClearTimer(ThinkTimer);
	if (UVeyraMovementComponent* Movement = WatchedMovement.Get())
	{
		Movement->OnMovementLockChanged.Remove(MovementLockHandle);
	}
	WatchedMovement.Reset();
	Target.Reset();
	Super::OnUnPossess();
}

void AVeyraCompanionController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ThinkTimer);
	Super::EndPlay(EndPlayReason);
}

void AVeyraCompanionController::Think()
{
	AVeyraCompanion* Body = GetCompanion();
	const FVeyraCompanionTuning* Tuning = Body ? Body->GetDefinition() : nullptr;
	UAbilitySystemComponent* Keeper = Body ? Body->GetOwnerAbilities() : nullptr;
	APawn* OwnerBody = Keeper ? AVeyraCompanion::BodyOf(*Keeper) : nullptr;
	// Banished, or with its owner fallen, it waits for its keeper to bring it back; anchored, it stands on (ADR-037 §1).
	const bool bAnchored = Tuning && Body->GetMode() == EVeyraCompanionMode::Anchored;
	if (!Tuning || !Body->IsAlive() || Body->IsBanished() || (!bAnchored && !Body->IsMoving() && !VeyraTargeting::IsAlive(OwnerBody)))
	{
		Target.Reset();
		Halt();
		return;
	}
	UVeyraBasicAttackComponent* Attacks = Body->GetBasicAttack();
	const UVeyraMovementComponent* Movement = WatchedMovement.Get();
	// It holds while something owns its movement, and lets an attack's windup finish.
	if (!Attacks || (Movement && Movement->IsMovementLocked()) || Attacks->GetState().Phase == EVeyraAttackPhase::Windup)
	{
		return;
	}
	// Anchored, it fights what its attack reaches from where it stands, and never walks (ADR-037 §1).
	if (bAnchored)
	{
		Halt();
		// A posture that holds its fire fights nothing (ADR-037 §2).
		if (Body->HoldsFire())
		{
			Target.Reset();
			return;
		}
		Target = const_cast<AActor*>(VeyraCompanionRules::Choose(EVeyraCompanionMode::Anchored, Target.Get(), GatherCandidates(*Body, OwnerBody, *Tuning)));
		AActor* Enemy = Target.Get();
		if (Enemy && Attacks->CheckAttack(Enemy) == EVeyraAttackRejection::None)
		{
			Attacks->StartAttack(*Enemy);
		}
		return;
	}
	// A hold ends with its time, or as its owner leaves the leash (ADR-034 §4).
	const double Now = GetWorld()->GetTimeSeconds();
	if (Body->GetMode() == EVeyraCompanionMode::Hold
		&& (Now >= Body->GetHoldsUntil() || FVector::Dist2D(Body->GetActorLocation(), OwnerBody->GetActorLocation()) > Tuning->LeashRange))
	{
		Body->EndHold();
	}

	// Summoned, it escorts its ally, whom its keeper helps now and then, falling back on its owner once the ally
	// is gone; or it hunts its enemy while that enemy stays within its leash of its owner (ADR-035 §5).
	if (Body->GetMode() == EVeyraCompanionMode::Escort)
	{
		AActor* Ally = Body->GetBoundTo();
		AActor* Leader = Ally && VeyraTargeting::IsAlive(Ally) ? Ally : VeyraTargeting::IsAlive(OwnerBody) ? OwnerBody : nullptr;
		// Moved with an ally, it fires as it goes at what its attack reaches, and keeps the way it was sent facing (ADR-037 §3).
		if (Body->IsMoving())
		{
			Target = const_cast<AActor*>(VeyraCompanionRules::Choose(EVeyraCompanionMode::Anchored, Target.Get(), GatherCandidates(*Body, OwnerBody, *Tuning)));
			AActor* Enemy = Target.Get();
			if (Enemy && Attacks->CheckAttack(Enemy) == EVeyraAttackRejection::None)
			{
				Halt();
				Attacks->StartAttack(*Enemy);
				return;
			}
			Body->FaceAnchor();
		}
		else
		{
			Target.Reset();
		}
		if (Leader)
		{
			Follow(*Leader, Tuning->FollowDistance);
		}
		else
		{
			Halt();
		}
		return;
	}
	if (Body->GetMode() == EVeyraCompanionMode::Hunt)
	{
		AActor* Prey = Body->GetBoundTo();
		if (Prey && VeyraTargeting::IsAlive(Prey) && VeyraTargeting::AreHostile(Body, Prey) && VeyraTargeting::CanAcquire(Body, *Prey)
			&& FVector::Dist2D(Prey->GetActorLocation(), OwnerBody->GetActorLocation()) <= Tuning->LeashRange)
		{
			Target = Prey;
			Engage(*Prey, *Attacks);
		}
		else
		{
			Target.Reset();
			Follow(*OwnerBody, Tuning->FollowDistance);
		}
		return;
	}

	Target = const_cast<AActor*>(VeyraCompanionRules::Choose(Body->GetMode(), Target.Get(), GatherCandidates(*Body, OwnerBody, *Tuning)));
	if (AActor* Enemy = Target.Get())
	{
		Engage(*Enemy, *Attacks);
	}
	else if (Body->GetMode() == EVeyraCompanionMode::Hold)
	{
		ReturnTo(Body->GetHoldPoint(), Body->GetSimpleCollisionRadius());
	}
	else
	{
		Follow(*OwnerBody, Tuning->FollowDistance);
	}
}

TArray<FVeyraCompanionCandidate> AVeyraCompanionController::GatherCandidates(const AVeyraCompanion& Body, const AActor* OwnerBody, const FVeyraCompanionTuning& Tuning) const
{
	TArray<FVeyraCompanionCandidate> Candidates;
	const UAbilitySystemComponent* Keeper = Body.GetOwnerAbilities();
	// Deployed, anchored or moved, it fights what its attack reaches, its owner's distance aside (ADR-037 §1, §3).
	const bool bAnchored = Body.GetMode() == EVeyraCompanionMode::Anchored || Body.IsMoving();
	if (!Keeper || (!bAnchored && !OwnerBody))
	{
		return Candidates;
	}
	// Following, it looks about itself; holding, about its point; anchored, as far as its attack reaches, its owner's
	// distance aside. A circle this wide touches every body within that range of its edge.
	const bool bHolding = Body.GetMode() == EVeyraCompanionMode::Hold;
	FVeyraShape Reach;
	Reach.Kind = EVeyraShapeKind::Circle;
	Reach.Radius = (bAnchored ? Tuning.BasicAttack.Range : Tuning.AcquireRange) + Body.GetSimpleCollisionRadius();
	const FVector Centre = bHolding ? Body.GetHoldPoint() : Body.GetActorLocation();
	const TOptional<FVector> OwnerAt = bAnchored ? TOptional<FVector>() : TOptional<FVector>(OwnerBody->GetActorLocation());
	const double Leash = Tuning.LeashRange;
	const TArray<AActor*> Units = VeyraShapes::GatherUnits(*GetWorld(), FVeyraPlacedShape{ Reach, Centre, Body.GetActorForwardVector() },
		[&Body, &OwnerAt, Leash](const AActor& Unit) {
			return VeyraTargeting::AreHostile(&Body, &Unit) && VeyraTargeting::CanAcquire(&Body, Unit)
				&& (!OwnerAt.IsSet() || FVector::Dist2D(Unit.GetActorLocation(), OwnerAt.GetValue()) <= Leash);
		});
	const double Now = GetWorld()->GetTimeSeconds();
	for (const AActor* Unit : Units)
	{
		FVeyraCompanionCandidate& Candidate = Candidates.Add_GetRef({ Unit, OwnerFoughtLately(*Unit, *Keeper, Now, Tuning.OwnerTargetSeconds),
			VeyraUnits::IsVanguard(Unit), VeyraTargeting::EdgeToEdgeDistance(Body, *Unit), Unit->GetUniqueID() });
		Candidate.bDesignated = VeyraCombat::HasStatusKindFrom(Unit, EVeyraStatusKind::Designated, *Keeper);
	}
	return Candidates;
}

void AVeyraCompanionController::Engage(AActor& Enemy, UVeyraBasicAttackComponent& Attacks)
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
		break;
	default:
		break;
	}
}

void AVeyraCompanionController::Follow(AActor& OwnerBody, double Distance)
{
	const APawn* Body = GetPawn();
	if (!Body || VeyraTargeting::EdgeToEdgeDistance(*Body, OwnerBody) <= Distance)
	{
		Halt();
		return;
	}
	// The path follows its owner as it moves; it stops once near again.
	if (Path == EPath::Owner && GetPathFollowingComponent()->GetStatus() != EPathFollowingStatus::Idle)
	{
		return;
	}
	const EPathFollowingRequestResult::Type Result = MoveToActor(&OwnerBody, static_cast<float>(Distance), /*bStopOnOverlap*/ true,
		/*bUsePathfinding*/ true, /*bCanStrafe*/ false, /*FilterClass*/ nullptr, /*bAllowPartialPath*/ true);
	Path = Result == EPathFollowingRequestResult::RequestSuccessful ? EPath::Owner : EPath::None;
}

void AVeyraCompanionController::ReturnTo(const FVector& Point, double Acceptance)
{
	const APawn* Body = GetPawn();
	if (!Body || FVector::Dist2D(Body->GetActorLocation(), Point) <= Acceptance)
	{
		Halt();
		return;
	}
	if (Path == EPath::Point && GetPathFollowingComponent()->GetStatus() != EPathFollowingStatus::Idle)
	{
		return;
	}
	const EPathFollowingRequestResult::Type Result = MoveToLocation(Point, static_cast<float>(Acceptance), /*bStopOnOverlap*/ false,
		/*bUsePathfinding*/ true, /*bProjectDestinationToNavigation*/ true, /*bCanStrafe*/ false, /*FilterClass*/ nullptr, /*bAllowPartialPath*/ true);
	Path = Result == EPathFollowingRequestResult::RequestSuccessful ? EPath::Point : EPath::None;
}

void AVeyraCompanionController::Halt()
{
	if (Path != EPath::None || GetPathFollowingComponent()->GetStatus() != EPathFollowingStatus::Idle)
	{
		Path = EPath::None;
		ChaseTarget.Reset();
		StopMovement();
	}
}

void AVeyraCompanionController::OnMovementLockChanged(bool bLocked)
{
	if (bLocked)
	{
		// Its next thought after the lock finds a path from wherever it was moved to.
		Halt();
	}
}

AVeyraCompanion* AVeyraCompanionController::GetCompanion() const
{
	return Cast<AVeyraCompanion>(GetPawn());
}
