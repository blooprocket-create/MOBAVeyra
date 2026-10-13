// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Navigation/PathFollowingComponent.h"
#include "VeyraPathFollowingComponent.generated.h"

/**
 * The path following every Veyra unit's server controller uses. It tells the unit's motion (ADR-079 §2) the corners its
 * path still has to walk, and where along the last of them it will stop, so every machine draws the body walking to the
 * very place the server's own stops.
 */
UCLASS()
class VEYRACOMBAT_API UVeyraPathFollowingComponent : public UPathFollowingComponent
{
	GENERATED_BODY()

public:
	/**
	 * The corners the body still walks, from the one it heads for now, with the last moved back along its leg to where the
	 * path counts the body arrived (its acceptance, the body's and its goal's radii, as the engine's reach test takes
	 * them). A last leg that heads for a moving goal ends at the goal's place now. False while it follows no path.
	 */
	bool GetCornersAhead(const FVector& From, TArray<FVector>& OutCorners) const;

	/** Every unit's controller's path following, given to it as its default subobject is made. */
	static FName DefaultName() { return TEXT("PathFollowingComponent"); }

private:
	/** How far from its goal's place the body counts as arrived, in 2D. */
	double ArrivalDistance() const;
};
