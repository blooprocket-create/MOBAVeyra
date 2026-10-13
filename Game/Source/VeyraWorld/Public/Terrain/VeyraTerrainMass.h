// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "GameFramework/Actor.h"

#include "VeyraTerrainMass.generated.h"

class UBoxComponent;
struct FVeyraWallRequest;

/**
 * One piece of the battleground's own terrain (ADR-073 §3): a rock or forest mass's edge ring, the keeps' walls or the rim,
 * as boxes that block every unit, forced moves and line projectiles as terrain does, and that the server's navigation
 * takes as obstacles so paths go round them. The layout is the same on every machine (ADR-011 §12), so each machine
 * builds its own and none replicates. Ability walls stay AVeyraTerrainWall (ADR-032 §4).
 */
UCLASS(NotBlueprintable, NotPlaceable)
class VEYRAWORLD_API AVeyraTerrainMass : public AActor
{
	GENERATED_BODY()

public:
	AVeyraTerrainMass();

	/** Once, after it joins the world: a box for each of Volumes, standing where each says, blocking at once. */
	void Build(TConstArrayView<FVeyraWallRequest> Volumes);

	TConstArrayView<TObjectPtr<UBoxComponent>> GetBoxes() const { return Boxes; }

private:
	UPROPERTY()
	TArray<TObjectPtr<UBoxComponent>> Boxes;
};
