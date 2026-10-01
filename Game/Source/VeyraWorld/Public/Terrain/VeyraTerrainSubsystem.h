// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "Terrain/VeyraRuntimeTerrain.h"

#include "VeyraTerrainSubsystem.generated.h"

class AVeyraTerrainWall;

/**
 * The battleground's runtime terrain (Battleground Bible §2, "Ability-created terrain"; ADR-003; ADR-032
 * §4): it implements Combat's terrain contract, raising each wall an ability asks for as a terrain wall
 * and lowering it on request. As a wall forms, each unit standing where it stands is moved to the nearest
 * legal point beside it, on the side its centre was on: a placement correction that deals no damage,
 * applies no crowd control and is not the unit's own move. Server only for raising and lowering.
 */
UCLASS()
class VEYRAWORLD_API UVeyraTerrainSubsystem : public UWorldSubsystem, public IVeyraRuntimeTerrain
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual int32 RaiseWall(const FVeyraWallRequest& Request) override;
	virtual void LowerWall(int32 Handle) override;

	/** How many walls stand. */
	int32 GetWallCount() const { return Walls.Num(); }

private:
	/** Moves each unit standing where Request's wall forms to the nearest legal point beside it. */
	void MoveOut(const FVeyraWallRequest& Request) const;

	TMap<int32, TWeakObjectPtr<AVeyraTerrainWall>> Walls;
	int32 NextHandle = 1;
};
