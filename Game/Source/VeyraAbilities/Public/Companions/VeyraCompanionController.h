// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "AIController.h"
#include "Engine/TimerHandle.h"

#include "VeyraCompanionController.generated.h"

class AVeyraCompanion;
class UVeyraBasicAttackComponent;
class UVeyraMovementComponent;
struct FVeyraCompanionCandidate;
struct FVeyraCompanionTuning;

/**
 * A companion's server-only controller (ADR-034 §4). On a world-time timer, so a pause holds it:
 * - following, it keeps near its owner and fights what its owner fought lately, within its owner's leash;
 * - holding, it fights what comes near its point, its owner's foes first, then Vanguards, and returns to
 *   the point between fights, until its time runs out or its owner leaves the leash;
 * - while crowd control locks its movement it holds, as a Fluxborn does.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class VEYRAABILITIES_API AVeyraCompanionController : public AAIController
{
	GENERATED_BODY()

public:
	AVeyraCompanionController(const FObjectInitializer& ObjectInitializer);

	/** Fights, follows or holds. Its timer calls it; tests may too. */
	void Think();

	AActor* GetTarget() const { return Target.Get(); }

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TArray<FVeyraCompanionCandidate> GatherCandidates(const AVeyraCompanion& Body, const AActor& OwnerBody, const FVeyraCompanionTuning& Tuning) const;
	void Engage(AActor& Enemy, UVeyraBasicAttackComponent& Attacks);
	void Follow(AActor& OwnerBody, double Distance);
	void ReturnTo(const FVector& Point, double Acceptance);
	void Halt();
	void OnMovementLockChanged(bool bLocked);
	AVeyraCompanion* GetCompanion() const;

	/** What its path follows now. */
	enum class EPath : uint8
	{
		None,
		Chase,
		Owner,
		Point,
	};

	TWeakObjectPtr<AActor> Target;
	TWeakObjectPtr<AActor> ChaseTarget;
	EPath Path = EPath::None;
	TWeakObjectPtr<UVeyraMovementComponent> WatchedMovement;
	FDelegateHandle MovementLockHandle;
	FTimerHandle ThinkTimer;
};
