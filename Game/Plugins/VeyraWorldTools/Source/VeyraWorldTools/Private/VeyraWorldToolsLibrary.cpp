// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraWorldToolsLibrary.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Terrain/VeyraSurfacePlacement.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"

bool UVeyraWorldToolsLibrary::StandingPoint(const UObject* WorldContext, FVector2D Point, FVector& OutLocation)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	return World && VeyraSurfacePlacement::Resolve(*World, Point, 0.0, UVeyraWorldTuningSubsystem::Get().Layout.Surface, OutLocation);
}
