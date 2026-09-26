// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Tuning/VeyraTuning.h"
#include "UObject/ObjectMacros.h"

#include "VeyraGreyboxLayout.generated.h"

class UWorld;

// The grey-box test map's layout, bound from Source/VeyraDeveloper/Greybox/Greybox.json with the
// tuning framework. UHT forbids preprocessor guards around USTRUCTs, so this header is unconditional.

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
};

namespace VeyraGreybox
{
	/** Reads and validates the layout file. */
	VeyraTuning::FErrors LoadLayout(FVeyraGreyboxLayout& OutLayout);

	/** The floor, which every machine needs so replicated movement has ground under it. */
	void SpawnFloor(UWorld& World, const FVeyraGreyboxLayout& Layout);

	/** The server's parts: each side's start and the navigation bounds. */
	void SpawnServerParts(UWorld& World, const FVeyraGreyboxLayout& Layout);
}
