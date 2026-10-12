// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Components/SceneComponent.h"
#include "Tuning/VeyraTuning.h"
#include "UObject/ObjectMacros.h"

#include "VeyraGreyboxLayout.generated.h"

class UWorld;

// A grey-box layout, bound from a file in Source/VeyraDeveloper/Greybox/ with the tuning framework: the test map's
// (Greybox.json), or the skill showcase's stage (Showcase.json, ADR-074 §1). UHT forbids preprocessor guards around
// USTRUCTs, so this header is unconditional.

USTRUCT()
struct FVeyraGreyboxFloor
{
	GENERATED_BODY()

	UPROPERTY()
	double LengthX = 0.0;

	UPROPERTY()
	double WidthY = 0.0;

	UPROPERTY()
	double ThicknessZ = 0.0;
};

USTRUCT()
struct FVeyraGreyboxTeamStarts
{
	GENERATED_BODY()

	UPROPERTY()
	double DistanceFromCenterX = 0.0;
};

USTRUCT()
struct FVeyraGreyboxNavigation
{
	GENERATED_BODY()

	UPROPERTY()
	double HeightZ = 0.0;
};

USTRUCT()
struct FVeyraGreyboxSun
{
	GENERATED_BODY()

	UPROPERTY()
	double PitchDegrees = 0.0;

	UPROPERTY()
	double YawDegrees = 0.0;
};

USTRUCT()
struct FVeyraGreyboxLayout
{
	GENERATED_BODY()

	static constexpr int32 SchemaVersion = 1;

	UPROPERTY()
	FVeyraGreyboxFloor Floor;

	UPROPERTY()
	FVeyraGreyboxTeamStarts TeamStarts;

	UPROPERTY()
	FVeyraGreyboxNavigation Navigation;

	UPROPERTY()
	FVeyraGreyboxSun Sun;
};

/**
 * Builds the grey-box layout. The map commandlet saves it as a map; the network tests build it in
 * each play session's worlds at runtime.
 */
namespace VeyraGreybox
{
	/** The test map's layout file, the one every caller reads unless it names another. */
	inline const TCHAR* const TestMapLayoutFile = TEXT("Greybox.json");

	/** Reads and validates LayoutFile, a file name in Source/VeyraDeveloper/Greybox/. */
	VeyraTuning::FErrors LoadLayout(FVeyraGreyboxLayout& OutLayout, const TCHAR* LayoutFile = TestMapLayoutFile);

	/** The size of the navigation bounds, centred on the origin. */
	FVector NavigationBoundsSize(const FVeyraGreyboxLayout& Layout);

	/**
	 * The floor. Every machine needs it so replicated movement has ground under it. A map saves it
	 * as static; a floor spawned during play must be movable to take its mesh.
	 */
	void SpawnFloor(UWorld& World, const FVeyraGreyboxLayout& Layout, EComponentMobility::Type Mobility);

	/** Each side's start. Only the server uses them. */
	void SpawnTeamStarts(UWorld& World, const FVeyraGreyboxLayout& Layout);

	/**
	 * Navigation bounds for a world that is already playing. A volume spawned then has no brush, so
	 * its bounds come from a transient collision box. The map commandlet builds a brush instead.
	 */
	void SpawnRuntimeNavigationBounds(UWorld& World, const FVeyraGreyboxLayout& Layout);
}
