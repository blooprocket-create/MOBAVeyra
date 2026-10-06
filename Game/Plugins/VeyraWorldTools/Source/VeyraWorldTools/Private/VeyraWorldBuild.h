// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class UWorld;
class FJsonObject;
struct FVeyraTerrainRelief;
struct FVeyraWorldTuning;

/** The Crucible's authoring passes (ADR-040): each builds one part of the map from World.json and the style profile. */
namespace VeyraWorldBuild
{
	/** The style profile's relief for the terrain field: presentation, never spatial authority. */
	FVeyraTerrainRelief ReliefOf(const FJsonObject& Style);

	bool Landscape(UWorld& World, const FVeyraWorldTuning& Tuning, const FJsonObject& Style, FString& Error);
	bool River(UWorld& World, const FVeyraWorldTuning& Tuning, const FJsonObject& Style, FString& Error);
	bool Dressing(UWorld& World, const FVeyraWorldTuning& Tuning, const FJsonObject& Style, FString& Error);
	void ReviewScene(UWorld& World, const FVeyraWorldTuning& Tuning, const FJsonObject& Style);
	bool Generate(UWorld& World, FString& Error);
}