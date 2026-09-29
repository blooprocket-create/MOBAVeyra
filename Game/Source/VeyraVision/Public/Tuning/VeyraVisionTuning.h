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

	/** A placed ward. Inside Dense Fog it sees no enemy Vanguard (Vision Bible §4). */
	UPROPERTY()
	double Ward = 0.0;
};

/** The Persistent Ward's carried charges (Vision Bible §4). */
USTRUCT()
struct FVeyraWardChargesTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** Charges carried at most, and at the start: a cap on charges, not on wards placed. */
	UPROPERTY()
	int32 Max = 0;
};

/** The Persistent Ward and the wards it places (Vision Bible §4, §9; ADR-016 §6). */
USTRUCT()
struct FVeyraPersistentWardTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** Seconds for one charge to come back while fewer than the most are carried. */
	UPROPERTY()
	double RechargeSeconds = 0.0;

	/** Seconds a placed ward lasts unless destroyed first. */
	UPROPERTY()
	double LifetimeSeconds = 0.0;

	/** How far from its Vanguard a ward is placed; a point further away is brought within it. */
	UPROPERTY()
	double PlacementRange = 0.0;

	/** Basic attacks from enemy Vanguards that destroy a ward, whatever their damage. */
	UPROPERTY()
	int32 HitsToDestroy = 0;

	/** A ward's body, which a player clicks to attack it: its radius, and half its height. */
	UPROPERTY()
	double BodyRadius = 0.0;

	UPROPERTY()
	double BodyHalfHeight = 0.0;
};

/** The Vision domain's tuning, bound from Game/Tuning/Vision.json (ADR-006 §6, ADR-016 §9). */
USTRUCT()
struct FVeyraVisionTuning
{
	GENERATED_BODY()

	/** The Vision.json format this build reads (a schema version marker, not tuning). */
	static constexpr int32 SchemaVersion = 2;

	UPROPERTY()
	FVeyraVisionUpdateTuning Update;

	UPROPERTY()
	FVeyraSightTuning Sight;

	UPROPERTY()
	FVeyraWardChargesTuning WardCharges;

	UPROPERTY()
	FVeyraPersistentWardTuning PersistentWard;
};

/** The Vision domain's checks that a schema cannot express. */
namespace VeyraVision
{
	/** Problems with Tuning, each a JSON pointer and a message; empty when it is consistent. */
	VEYRAVISION_API TArray<FString> Validate(const FVeyraVisionTuning& Tuning);
}
