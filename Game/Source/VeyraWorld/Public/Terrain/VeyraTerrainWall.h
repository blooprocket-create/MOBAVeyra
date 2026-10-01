// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "GameFramework/Actor.h"

#include "VeyraTerrainWall.generated.h"

class UBoxComponent;

/**
 * A wall of runtime terrain (Battleground Bible §2, "Ability-created terrain"; ADR-032 §4), as Forge
 * Divide's cooled black iron: a world-static box that blocks every unit, forced moves and line projectiles
 * as terrain does, and that the server's navigation takes as an obstacle so paths go round it. Its size
 * replicates, so every machine's movement meets it; every player sees it, as map knowledge. The terrain
 * subsystem raises and lowers it; what owns it is the ability's marker.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class VEYRAWORLD_API AVeyraTerrainWall : public AActor
{
	GENERATED_BODY()

public:
	AVeyraTerrainWall();

	/** Server, before it joins the world: half its thickness along its facing, half its length across it, and half its height. */
	void SetHalfExtent(const FVector& InHalfExtent);

	FVector GetHalfExtent() const { return FVector(HalfExtent); }

	/** Server, once it has joined the world: it takes its shape and blocks. Every machine also forms it as play begins or its size arrives. */
	void Form() { ApplyHalfExtent(); }

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/** Shapes its body from HalfExtent and lets it block, on every machine. */
	UFUNCTION()
	void ApplyHalfExtent();

	UPROPERTY(ReplicatedUsing = ApplyHalfExtent)
	FVector3f HalfExtent = FVector3f::ZeroVector;

	/** Blocks nothing until ApplyHalfExtent shapes it. */
	UPROPERTY()
	TObjectPtr<UBoxComponent> Box;
};
