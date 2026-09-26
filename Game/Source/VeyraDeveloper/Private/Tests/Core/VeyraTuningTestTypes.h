// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "UObject/ObjectMacros.h"

#include "VeyraTuningTestTypes.generated.h"

// Test-only tuning shapes. They match TestData/TuningConformance.schema.json,
// TestData/TuningConformanceContent.schema.json and the schemas the Veyra.Core.Tuning tests build.
// UHT forbids preprocessor guards around USTRUCTs, so this header is unconditional.

USTRUCT()
struct FVeyraTuningTestNested
{
	GENERATED_BODY()

	UPROPERTY()
	double Ratio = 0.0;
};

USTRUCT()
struct FVeyraTuningTestShape
{
	GENERATED_BODY()

	UPROPERTY()
	double Speed = 0.0;

	UPROPERTY()
	int32 Count = 0;

	UPROPERTY()
	FVeyraTuningTestNested Nested;
};

UENUM()
enum class EVeyraTuningTestKind : uint8
{
	Alpha,
	Beta,
};

USTRUCT()
struct FVeyraTuningTestEntry
{
	GENERATED_BODY()

	UPROPERTY()
	double Weight = 0.0;
};

/** Strings and content-keyed maps: matches TestData/TuningConformanceContent.schema.json. */
USTRUCT()
struct FVeyraTuningContentTestShape
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningTestKind Kind = EVeyraTuningTestKind::Alpha;

	UPROPERTY()
	FVeyraContentId Target;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraTuningTestEntry> Entries;
};
