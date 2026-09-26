// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "GameFramework/PlayerController.h"
#include "VeyraMatchTypes.h"

#include "VeyraPlayerController.generated.h"

class AVeyraVanguardCharacter;

/**
 * A human player's connection to the match. It possesses nothing: it sends the player's intents to
 * the server, which validates them and drives the Vanguard through its AVeyraVanguardController
 * (ADR-006 §7). On the owning client it views the Vanguard.
 */
UCLASS()
class VEYRAMATCH_API AVeyraPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AVeyraPlayerController(const FObjectInitializer& ObjectInitializer);

	/** Owning client: asks the server to move this player's Vanguard to Destination. */
	void IssueMoveOrder(const FVector& Destination);

	/**
	 * Owning client, developer builds: asks the server to pause or resume the match at once. Pause
	 * votes (Match Flow Bible §10) will replace it; Shipping servers refuse it.
	 */
	void RequestDeveloperPause(bool bPause);

	/** This player's Vanguard, on the server and on every client, or null before it spawns. */
	AVeyraVanguardCharacter* GetVanguard() const;

	/** Owning client: the reason the server gave for the last refused order, and how many it refused. */
	EVeyraOrderRejection GetLastOrderRejection() const { return LastOrderRejection; }
	int32 GetOrderRejectionCount() const { return OrderRejectionCount; }

	/**
	 * On the server this is the Vanguard's position, never the location a pawn-less client reports:
	 * replication decides what each player receives from it.
	 */
	virtual void GetPlayerViewPoint(FVector& OutLocation, FRotator& OutRotation) const override;

protected:
	virtual void OnRep_PlayerState() override;

private:
	UFUNCTION(Server, Reliable)
	void ServerIssueMoveOrder(FVector Destination);

	UFUNCTION(Client, Unreliable)
	void ClientOrderRejected(EVeyraOrderRejection Rejection);

	UFUNCTION(Server, Reliable)
	void ServerRequestDeveloperPause(bool bPause);

	UFUNCTION()
	void OnVanguardSet(APlayerState* Participant, APawn* NewPawn, APawn* OldPawn);

	void RejectOrder(EVeyraOrderRejection Rejection);

	/** Server: spends one order from the player's allowance, refilled at the tuned rate. */
	bool TakeOrderAllowance();

	double OrderAllowance = 0.0;
	double OrderAllowanceTime = 0.0;
	bool bOrderAllowanceStarted = false;

	EVeyraOrderRejection LastOrderRejection = EVeyraOrderRejection::None;
	int32 OrderRejectionCount = 0;
};
