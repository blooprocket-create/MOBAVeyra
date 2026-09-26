// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "UObject/ObjectMacros.h"

#include "VeyraTuningTestTypes.generated.h"

// Test-only tuning shapes. They match TestData/TuningConformance.schema.json and the schemas the
// Veyra.Core.Tuning tests build. UHT forbids preprocessor guards around USTRUCTs, so this header
// is unconditional.

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
