// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Terrain/VeyraRuntimeTerrain.h"

#include "Engine/World.h"
#include "VeyraCombatLog.h"

void UVeyraRuntimeTerrainRegistry::Register(IVeyraRuntimeTerrain& InTerrain)
{
	Terrain = &InTerrain;
}

void UVeyraRuntimeTerrainRegistry::Unregister(const IVeyraRuntimeTerrain& InTerrain)
{
	if (Terrain == &InTerrain)
	{
		Terrain = nullptr;
	}
}

namespace VeyraRuntimeTerrain
{
int32 RaiseWall(UWorld& World, const FVeyraWallRequest& Request)
{
	const UVeyraRuntimeTerrainRegistry* Registry = World.GetSubsystem<UVeyraRuntimeTerrainRegistry>();
	IVeyraRuntimeTerrain* Terrain = Registry ? Registry->Get() : nullptr;
	if (!Terrain)
	{
		UE_LOG(LogVeyraCombat, Warning, TEXT("No runtime terrain governs %s: the wall at %s is not raised."), *World.GetName(), *Request.Centre.ToString());
		return 0;
	}
	return Terrain->RaiseWall(Request);
}

void LowerWall(UWorld& World, int32 Handle)
{
	const UVeyraRuntimeTerrainRegistry* Registry = World.GetSubsystem<UVeyraRuntimeTerrainRegistry>();
	if (IVeyraRuntimeTerrain* Terrain = Registry && Handle != 0 ? Registry->Get() : nullptr)
	{
		Terrain->LowerWall(Handle);
	}
}
}
