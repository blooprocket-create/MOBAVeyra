// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Wildlife/VeyraWildlifeController.h"

#include "AbilitySystemComponent.h"
#include "Attacks/VeyraBasicAttackComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Engine/World.h"
#include "Movement/VeyraMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Rules/VeyraWildlifeRules.h"
#include "Targeting/VeyraTargeting.h"
#include "TimerManager.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "Wildlife/VeyraWildlife.h"

AVeyraWildlifeController::AVeyraWildlifeController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The server moves every creature; clients never see their controllers.
	bReplicates = false;
	bSetControlRotationFromPawnOrientation = false;
}

void AVeyraWildlifeController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (UVeyraMovementComponent* Movement = InPawn ? Cast<UVeyraMovementComponent>(InPawn->GetMovementComponent()) : nullptr)
	{
		WatchedMovement = Movement;
		MovementLockHandle = Movement->OnMovementLockChanged.AddUObject(this, &AVeyraWildlifeController::OnMovementLockChanged);
	}
	// On world time, so a pause holds it (ADR-006 §8).
	const float Cadence = static_cast<float>(UVeyraWorldTuningSubsystem::Get().Wildlife.Ai.ThinkSeconds);
	GetWorldTimerManager().SetTimer(ThinkTimer, FTimerDelegate::CreateUObject(this, &AVeyraWildlifeController::Think), Cadence, /*bLoop*/ true);
}

void AVeyraWildlifeController::OnUnPossess()
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

void AVeyraWildlifeController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ThinkTimer);
	Super::EndPlay(EndPlayReason);
}

void AVeyraWildlifeController::NoteAggression(AActor& Attacker)
{
	const AVeyraWildlife* Body = GetWildlife();
	if (!Body)
	{
		return;
	}
	// Walking home, it answers only an attacker within its leash, or it would be drawn straight back out.
	if (bReturning && !VeyraWildlifeRules::IsWithinLeash(Body->GetLeashCenter(), Body->GetLeashRadius(), FVector2D(Attacker.GetActorLocation())))
	{
		return;
	}
	bReturning = false;
	Target = &Attacker;
}

void AVeyraWildlifeController::Think()
{
	AVeyraWildlife* Body = GetWildlife();
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
	if (bReturning)
	{
		WalkHome(*Body);
		return;
	}
	AActor* Enemy = Target.Get();
	if (!Enemy)
	{
		// Unprovoked, it waits at its spot.
		return;
	}
	const bool bValid = VeyraTargeting::IsAlive(Enemy) && VeyraTargeting::AreHostile(Body, Enemy);
	if (!VeyraWildlifeRules::KeepsFighting(Body->GetLeashCenter(), Body->GetLeashRadius(), FVector2D(Body->GetActorLocation()),
			FVector2D(Enemy->GetActorLocation()), bValid))
	{
		// It gives up: home, to heal (Battleground Bible §17).
		Target.Reset();
		Attacks->CancelAttack();
		Halt();
		bReturning = true;
		WalkHome(*Body);
		return;
	}
	Engage(*Enemy, *Attacks);
}

void AVeyraWildlifeController::Engage(AActor& Enemy, UVeyraBasicAttackComponent& Attacks)
{
	switch (Attacks.CheckAttack(&Enemy))
	{
	case EVeyraAttackRejection::None:
		Halt();
		Attacks.StartAttack(Enemy);
		break;
	case EVeyraAttackRejection::OutOfRange:
		if (!bMoving || ChaseTarget.Get() != &Enemy)
		{
			// Closing cuts a backswing short; the path follows the enemy and a later thought stops it in range.
			Attacks.CancelAttack();
			const EPathFollowingRequestResult::Type Result = MoveToActor(&Enemy, /*AcceptanceRadius*/ 0.0f, /*bStopOnOverlap*/ true,
				/*bUsePathfinding*/ true, /*bCanStrafe*/ false, /*FilterClass*/ nullptr, /*bAllowPartialPath*/ true);
			bMoving = Result == EPathFollowingRequestResult::RequestSuccessful;
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

void AVeyraWildlifeController::WalkHome(AVeyraWildlife& Body)
{
	const double Acceptance = UVeyraWorldTuningSubsystem::Get().Wildlife.Ai.HomeAcceptance;
	const FVector& Home = Body.GetHome();
	bool bHome = FVector::Dist2D(Body.GetActorLocation(), Home) <= Acceptance;
	if (!bHome)
	{
		if (bMoving && !ChaseTarget.IsValid() && GetPathFollowingComponent()->GetStatus() != EPathFollowingStatus::Idle)
		{
			return;
		}
		const EPathFollowingRequestResult::Type Result = MoveToLocation(Home, static_cast<float>(Acceptance), /*bStopOnOverlap*/ false,
			/*bUsePathfinding*/ true, /*bProjectDestinationToNavigation*/ true, /*bCanStrafe*/ false, /*FilterClass*/ nullptr, /*bAllowPartialPath*/ true);
		bMoving = Result == EPathFollowingRequestResult::RequestSuccessful;
		ChaseTarget.Reset();
		// The path follower may judge it home when this measure does not.
		bHome = Result == EPathFollowingRequestResult::AlreadyAtGoal;
	}
	if (bHome)
	{
		// Home, it is whole again: a reset, as League's camps reset.
		bReturning = false;
		Halt();
		UAbilitySystemComponent& AbilitySystem = *Body.GetAbilitySystemComponent();
		VeyraCombat::RestoreHealth(AbilitySystem, AbilitySystem.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()));
	}
}

void AVeyraWildlifeController::Halt()
{
	if (bMoving || GetPathFollowingComponent()->GetStatus() != EPathFollowingStatus::Idle)
	{
		bMoving = false;
		ChaseTarget.Reset();
		StopMovement();
	}
}

void AVeyraWildlifeController::OnMovementLockChanged(bool bLocked)
{
	if (bLocked)
	{
		Halt();
	}
}

AVeyraWildlife* AVeyraWildlifeController::GetWildlife() const
{
	return Cast<AVeyraWildlife>(GetPawn());
}
