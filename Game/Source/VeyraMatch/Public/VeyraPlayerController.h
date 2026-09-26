// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "GameFramework/PlayerController.h"
#include "Input/VeyraInputSettings.h"
#include "VeyraAbilityTypes.h"
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

	/** Owning client: asks the server to cast the ability in Slot at Target. */
	void IssueCastOrder(EVeyraAbilitySlot Slot, AActor* Target);

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

	/** Owning client: the reason the server gave for the last refused cast, and how many it refused. */
	EVeyraCastRejection GetLastCastRejection() const { return LastCastRejection; }
	int32 GetCastRejectionCount() const { return CastRejectionCount; }

	/**
	 * On the server this is the Vanguard's position, never the location a pawn-less client reports:
	 * replication decides what each player receives from it.
	 */
	virtual void GetPlayerViewPoint(FVector& OutLocation, FRotator& OutRotation) const override;

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void OnRep_PlayerState() override;

private:
	UFUNCTION(Server, Reliable)
	void ServerIssueMoveOrder(FVector Destination);

	UFUNCTION(Client, Unreliable)
	void ClientOrderRejected(EVeyraOrderRejection Rejection);

	UFUNCTION(Server, Reliable)
	void ServerIssueCastOrder(EVeyraAbilitySlot Slot, FVeyraCastTarget Target);

	UFUNCTION(Client, Unreliable)
	void ClientCastRejected(EVeyraCastRejection Rejection);

	UFUNCTION(Server, Reliable)
	void ServerRequestDeveloperPause(bool bPause);

	UFUNCTION()
	void OnVanguardSet(APlayerState* Participant, APawn* NewPawn, APawn* OldPawn);

	void RejectOrder(EVeyraOrderRejection Rejection);

	// Local input (Settings Bible §1): right-click move and Quick Cast on Q.
	void OnMoveOrderStarted();
	void OnMoveOrderHeld();
	void OnAbilityQ();
	void MoveToCursor();

	UPROPERTY(Transient)
	FVeyraInputObjects Input;

	double LastHeldMoveOrderTime = 0.0;

	/** Server: spends one order from the player's allowance, refilled at the tuned rate. */
	bool TakeOrderAllowance();

	double OrderAllowance = 0.0;
	double OrderAllowanceTime = 0.0;
	bool bOrderAllowanceStarted = false;

	EVeyraOrderRejection LastOrderRejection = EVeyraOrderRejection::None;
	int32 OrderRejectionCount = 0;

	EVeyraCastRejection LastCastRejection = EVeyraCastRejection::None;
	int32 CastRejectionCount = 0;
};
