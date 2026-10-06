// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "VeyraWorldToolsLibrary.generated.h"

/** The battleground's own rules for editor scripts (ADR-040): queries they call rather than copy. */
UCLASS()
class UVeyraWorldToolsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Where a unit's feet stand at Point in WorldContext's world: on its playable ground, through VeyraSurfacePlacement
	 * with the World layout's surface rules, as the game places units. False where there is no walkable ground.
	 */
	UFUNCTION(BlueprintCallable, Category = "Veyra|World", meta = (WorldContext = "WorldContext"))
	static bool StandingPoint(const UObject* WorldContext, FVector2D Point, FVector& OutLocation);
};
