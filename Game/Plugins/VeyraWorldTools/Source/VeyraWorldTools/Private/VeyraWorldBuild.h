// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class UWorld;
class FJsonObject;
class FVeyraTerrainField;
struct FVeyraTerrainRelief;
struct FVeyraWorldTuning;

/** The floor under a mass's outline: its lowest, which its first stratum reaches below, and its highest, its base. */
struct FMassGround
{
	double Low = 0.0;
	double Base = 0.0;
};

/** The Crucible's authoring passes (ADR-040): each builds one part of the map from World.json and the style profile. */
namespace VeyraWorldBuild
{
	/** The style profile's relief for the terrain field: presentation, never spatial authority. */
	FVeyraTerrainRelief ReliefOf(const FJsonObject& Style);

	/** The masses' look (Config/CrucibleMassArt.json, ADR-073 §5), refused when it was built from other masses than Tuning's. */
	TSharedPtr<FJsonObject> LoadMassArt(const FVeyraWorldTuning& Tuning, FString& Error);

	/** The floor along Outline, as Field gives it. */
	FMassGround MassGround(const FVeyraTerrainField& Field, TConstArrayView<FVector2D> Outline);

	bool Landscape(UWorld& World, const FVeyraWorldTuning& Tuning, const FJsonObject& Style, FString& Error);
	bool River(UWorld& World, const FVeyraWorldTuning& Tuning, const FJsonObject& Style, FString& Error);

	/**
	 * The rock and forest masses (ADR-073 §5): Team A's each a mesh of its strata over its gameplay outline, with its ink
	 * lines, and Team B's the same meshes turned half a turn. Presentation: none of it blocks.
	 */
	bool Masses(UWorld& World, const FVeyraWorldTuning& Tuning, const FJsonObject& Style, FString& Error);

	/**
	 * Each keep's architecture (ADR-073 §5): its walls over exactly the terrain that blocks there, crenellated, with a
	 * pillar at each wall's end beside a gate. Team B's is Team A's turned half a turn. Presentation: none of it blocks.
	 */
	bool Keeps(UWorld& World, const FVeyraWorldTuning& Tuning, const FJsonObject& Style, FString& Error);
	bool Dressing(UWorld& World, const FVeyraWorldTuning& Tuning, const FJsonObject& Style, FString& Error);
	void ReviewScene(UWorld& World, const FVeyraWorldTuning& Tuning, const FJsonObject& Style);
	bool Generate(UWorld& World, FString& Error);
}