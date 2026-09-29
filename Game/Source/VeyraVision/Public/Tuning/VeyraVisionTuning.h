// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Tuning/VeyraTuningProvenance.h"
#include "UObject/ObjectMacros.h"

#include "VeyraVisionTuning.generated.h"

/** How often the server works out what each team and player sees (ADR-016 §2). */
USTRUCT()
struct FVeyraVisionUpdateTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** Seconds between two passes; above 0. A unit stepping into sight shows at most this late. */
	UPROPERTY()
	double UpdateSeconds = 0.0;
};

/** How far each kind of unit sees, in units (Vision Bible §1). */
USTRUCT()
struct FVeyraSightTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	UPROPERTY()
	double Vanguard = 0.0;

	UPROPERTY()
	double Fluxborn = 0.0;

	/** Spires, base towers, inhibitors and the Prime Well. */
	UPROPERTY()
	double Structure = 0.0;
};

/** The Vision domain's tuning, bound from Game/Tuning/Vision.json (ADR-006 §6, ADR-016 §9). */
USTRUCT()
struct FVeyraVisionTuning
{
	GENERATED_BODY()

	/** The Vision.json format this build reads (a schema version marker, not tuning). */
	static constexpr int32 SchemaVersion = 1;

	UPROPERTY()
	FVeyraVisionUpdateTuning Update;

	UPROPERTY()
	FVeyraSightTuning Sight;
};

/** The Vision domain's checks that a schema cannot express. */
namespace VeyraVision
{
	/** Problems with Tuning, each a JSON pointer and a message; empty when it is consistent. */
	VEYRAVISION_API TArray<FString> Validate(const FVeyraVisionTuning& Tuning);
}
