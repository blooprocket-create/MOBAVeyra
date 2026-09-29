// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "AIController.h"
#include "Engine/TimerHandle.h"
#include "Rules/VeyraFluxbornRules.h"

#include "VeyraFluxbornController.generated.h"

class AVeyraFluxborn;
class UVeyraBasicAttackComponent;
class UVeyraMovementComponent;

/**
 * A Fluxborn's server-only controller (ADR-011 §7). On a world-time timer, so a pause holds it, it
 * picks a target under VeyraFluxbornRules among the enemies within its acquisition radius and its
 * leash of the lane, then attacks it, closes on it, or walks the lane's waypoints toward the enemy
 * base; after a chase it resumes at the first waypoint ahead. While crowd control locks its movement
 * it holds, like a Vanguard's controller.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class VEYRAWORLD_API AVeyraFluxbornController : public AAIController
{
	GENERATED_BODY()

public:
	AVeyraFluxbornController(const FObjectInitializer& ObjectInitializer);

	/** Picks its target and attacks, closes or walks. Its timer calls it; tests may too. */
	void Think();

	/**
	 * Attacker, an enemy Vanguard, hurt an allied Vanguard near this Fluxborn (Battleground Bible
	 * §19). It may answer at its next thought.
	 */
	void NoteAggression(AActor& Attacker);

	/** The living, damageable enemies it could attack now. */
	TArray<FVeyraFluxbornCandidate> GatherCandidates() const;

	AActor* GetTarget() const { return Target.Get(); }
	bool IsResponding() const { return bResponding; }

	/** The waypoint it walks toward, an index into its Fluxborn's waypoints. */
	int32 GetWaypointIndex() const { return WaypointIndex; }

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** How it is moving now. */
	enum class EPath : uint8
	{
		None,
		Lane,
		Chase,
	};

	void Engage(AActor& Enemy, UVeyraBasicAttackComponent& Attacks);
	void Walk(const AVeyraFluxborn& Body);
	void Halt();
	void OnMovementLockChanged(bool bLocked);
	AVeyraFluxborn* GetFluxborn() const;

	TWeakObjectPtr<AActor> Target;
	bool bResponding = false;
	TWeakObjectPtr<AActor> Claimant;
	int32 WaypointIndex = 1;
	EPath Path = EPath::None;
	TWeakObjectPtr<AActor> ChaseTarget;
	/** Whether it left its lane to fight, so it must find the waypoint ahead before walking on. */
	bool bOffLane = false;
	TWeakObjectPtr<UVeyraMovementComponent> WatchedMovement;
	FDelegateHandle MovementLockHandle;
	FTimerHandle ThinkTimer;
};
