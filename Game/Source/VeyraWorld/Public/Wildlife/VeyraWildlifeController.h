// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "AIController.h"
#include "Engine/TimerHandle.h"

#include "VeyraWildlifeController.generated.h"

class AVeyraWildlife;
class UVeyraBasicAttackComponent;
class UVeyraMovementComponent;

/**
 * A creature's server-only controller (ADR-014 §2). On a world-time timer, so a pause holds it:
 * - it waits at its spot until its camp is attacked, then fights the latest Vanguard to hurt the camp;
 * - it gives up when it or its target leaves the camp's leash, or its target can no longer be fought,
 *   and walks home, where it heals to full (Battleground Bible §17; a reset);
 * - walking home it answers only an attacker within the leash;
 * - while crowd control locks its movement it holds, as a Fluxborn does.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class VEYRAWORLD_API AVeyraWildlifeController : public AAIController
{
	GENERATED_BODY()

public:
	AVeyraWildlifeController(const FObjectInitializer& ObjectInitializer);

	/** Fights, gives up or walks home. Its timer calls it; tests may too. */
	void Think();

	/** Attacker hurt a creature of this one's camp. It fights them from its next thought. */
	void NoteAggression(AActor& Attacker);

	AActor* GetTarget() const { return Target.Get(); }

	/** Whether it has given up and walks home to heal. */
	bool IsReturning() const { return bReturning; }

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void Engage(AActor& Enemy, UVeyraBasicAttackComponent& Attacks);
	void WalkHome(AVeyraWildlife& Body);
	void Halt();
	void OnMovementLockChanged(bool bLocked);
	AVeyraWildlife* GetWildlife() const;

	TWeakObjectPtr<AActor> Target;
	TWeakObjectPtr<AActor> ChaseTarget;
	bool bReturning = false;
	bool bMoving = false;
	TWeakObjectPtr<UVeyraMovementComponent> WatchedMovement;
	FDelegateHandle MovementLockHandle;
	FTimerHandle ThinkTimer;
};
