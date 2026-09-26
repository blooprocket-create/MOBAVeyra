// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "GameFramework/Character.h"

#include "VeyraLaneStandIn.generated.h"

/**
 * A load-test stand-in for a lane unit (ADR-006 §5 bandwidth spike): a replicated character that
 * the server walks back and forth along its lane, with no controller and no gameplay. It stands
 * in for the network and movement cost of Fluxborn until lanes are built.
 */
UCLASS(NotPlaceable)
class AVeyraLaneStandIn : public ACharacter
{
	GENERATED_BODY()

public:
	AVeyraLaneStandIn(const FObjectInitializer& ObjectInitializer);

	/** Server only: walks between From and To, starting toward To, at Speed. */
	void Walk(const FVector& From, const FVector& To, float Speed);

	virtual void Tick(float DeltaSeconds) override;

private:
	FVector LaneFrom = FVector::ZeroVector;
	FVector LaneTo = FVector::ZeroVector;
	bool bTowardTo = true;
	bool bWalking = false;
};
