// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Camera/VeyraCameraPreferences.h"
#include "Camera/VeyraCameraRules.h"
#include "GameFramework/Actor.h"

#include "VeyraCameraRig.generated.h"

class UCameraComponent;
class USpringArmComponent;

/**
 * The local player's view (Settings Bible §2; ADR-020 §1): a client-only actor at the point the
 * camera looks at, with the same arm and pitch the Vanguard's view had (UVeyraCameraSettings). The
 * local PlayerController views through it and moves it with VeyraCamera's rules. It never replicates
 * and decides nothing about the match.
 */
UCLASS(Transient, NotPlaceable)
class VEYRAMATCH_API AVeyraCameraRig : public AActor
{
	GENERATED_BODY()

public:
	AVeyraCameraRig();

	virtual void PostInitializeComponents() override;

	/** Moves the focus for one frame, at the speeds Preferences give, or the developer's without them. */
	void Step(const FVeyraCameraInput& Input, double DeltaSeconds, const FVeyraCameraPreferences* Preferences = nullptr);

	/** Puts the focus on Point at once, as a minimap click or the end-of-match pan does. */
	void LookAt(const FVector& Point);

	/**
	 * Centres the view on Point, a new body's place, and drops how far a Semi-Locked camera had been
	 * pushed off the old one, so the view stays on the new body (ADR-020 §1).
	 */
	void CenterOn(const FVector& Point);

	EVeyraCameraMode GetMode() const { return Mode; }
	void SetMode(EVeyraCameraMode InMode) { Mode = InMode; }

	/** The point the camera looks at. */
	FVector GetFocus() const { return GetActorLocation(); }

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USpringArmComponent> Arm;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> Camera;

	EVeyraCameraMode Mode = EVeyraCameraMode::Free;
	FVector Offset = FVector::ZeroVector;
};
