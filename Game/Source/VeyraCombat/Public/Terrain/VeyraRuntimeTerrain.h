// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"

#include "VeyraRuntimeTerrain.generated.h"

class UWorld;

/** A wall an ability raises (ADR-032 §4): where it stands, which way it faces, and its size. */
struct FVeyraWallRequest
{
	/** Its centre, at the height of the bodies it blocks. */
	FVector Centre = FVector::ZeroVector;

	/** The way it faces, on the ground: its thickness runs along it and its length across it. */
	FVector Facing = FVector::ForwardVector;

	double Length = 0.0;
	double Thickness = 0.0;

	/** Half its height: as tall as the bodies it blocks. */
	double HalfHeight = 0.0;
};

/**
 * Terrain an ability raises at runtime (Battleground Bible §2, "Ability-created terrain"; ADR-003; ADR-032
 * §4): the contract the battleground implements, as the navigation system that owns runtime terrain, so
 * an ability can raise a wall without reaching into navigation. A wall is real terrain while it stands:
 * it blocks every unit of both teams and neutral wildlife, forced moves and line projectiles, and paths go
 * round it; a unit standing where it forms is moved out to the nearest legal point, which is a placement
 * correction, neither a hit nor the unit's own move.
 */
class IVeyraRuntimeTerrain
{
public:
	virtual ~IVeyraRuntimeTerrain() = default;

	/** Server: raises Request's wall and moves out what stands in it. Its handle, or 0 if none was raised. */
	virtual int32 RaiseWall(const FVeyraWallRequest& Request) = 0;

	/** Server: removes the wall Handle names; pathing is restored at once. */
	virtual void LowerWall(int32 Handle) = 0;
};

/** Holds a world's runtime terrain, if it has any: the battleground registers itself here on the server. */
UCLASS()
class VEYRACOMBAT_API UVeyraRuntimeTerrainRegistry : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** InTerrain governs this world until Unregister; it must outlive that. */
	void Register(IVeyraRuntimeTerrain& InTerrain);
	void Unregister(const IVeyraRuntimeTerrain& InTerrain);
	IVeyraRuntimeTerrain* Get() const { return Terrain; }

private:
	IVeyraRuntimeTerrain* Terrain = nullptr;
};

/** Raising and lowering a world's runtime terrain; a world with none raises nothing. */
namespace VeyraRuntimeTerrain
{
	/** Server: World's terrain raises Request's wall. Its handle, or 0 where the world raises none. */
	VEYRACOMBAT_API int32 RaiseWall(UWorld& World, const FVeyraWallRequest& Request);

	/** Server: World's terrain removes the wall Handle names. */
	VEYRACOMBAT_API void LowerWall(UWorld& World, int32 Handle);
}
