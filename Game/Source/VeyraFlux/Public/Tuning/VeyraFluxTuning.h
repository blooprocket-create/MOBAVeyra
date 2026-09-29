// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Tuning/VeyraTuningProvenance.h"
#include "UObject/ObjectMacros.h"

#include "VeyraFluxTuning.generated.h"

/** Whether a grant of Team Flux lasts the match or falls away after a time (Battleground Bible §9, §10). */
UENUM()
enum class EVeyraFluxDuration : uint8
{
	/** Never expires during the match. */
	Permanent,
	/** Expires after its own duration, independently of every other grant. */
	Temporary,
};

/** What one source grants its team. */
USTRUCT()
struct FVeyraFluxGrantTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraFluxDuration Duration = EVeyraFluxDuration::Permanent;

	/** Team Flux granted; above 0. */
	UPROPERTY()
	double Amount = 0.0;

	/** How long a temporary grant lasts, in seconds; 0 for a permanent one (VeyraFlux::Validate). */
	UPROPERTY()
	double DurationSeconds = 0.0;
};

/** What each source of Team Flux grants (Battleground Bible §5, §6, §10, §18). */
USTRUCT()
struct FVeyraFluxGrantsTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** A destroyed enemy lane Spire. */
	UPROPERTY()
	FVeyraFluxGrantTuning LaneSpire;

	/** A destroyed enemy base-defense tower. */
	UPROPERTY()
	FVeyraFluxGrantTuning BaseTower;

	/** A destroyed enemy inhibitor. */
	UPROPERTY()
	FVeyraFluxGrantTuning Inhibitor;

	/** A secured Flux Well (§6: +50 temporary Team Flux for 3 minutes). */
	UPROPERTY()
	FVeyraFluxGrantTuning FluxWell;
};

/**
 * How active Team Flux strengthens every allied lane Fluxborn (Battleground Bible §4): each full step
 * of active Flux adds these fractions of Health and damage.
 */
USTRUCT()
struct FVeyraFluxbornScalingTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** Active Flux per step; above 0. */
	UPROPERTY()
	double StepFlux = 0.0;

	/** Max Health added per step, as a fraction of the Fluxborn's base; 0.05 is 5%. */
	UPROPERTY()
	double HealthPerStep = 0.0;

	/** Damage added per step, as a fraction. */
	UPROPERTY()
	double DamagePerStep = 0.0;
};

/**
 * The permanent Team Flux that unlocks each Flux Spell slot (Battleground Bible §14; ADR-015 §4):
 * one threshold per slot, in slot order. Temporary Flux never counts.
 */
USTRUCT()
struct FVeyraSpellSlotsTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** Rising: a later slot never opens before an earlier one. */
	UPROPERTY()
	TArray<double> Thresholds;
};

/** The Flux domain's tuning, bound from Game/Tuning/Flux.json (ADR-006 §6, ADR-011 §10). */
USTRUCT()
struct FVeyraFluxTuning
{
	GENERATED_BODY()

	/** The Flux.json format this build reads (a schema version marker, not tuning). */
	static constexpr int32 SchemaVersion = 3;

	UPROPERTY()
	FVeyraFluxGrantsTuning Grants;

	UPROPERTY()
	FVeyraFluxbornScalingTuning FluxbornScaling;

	UPROPERTY()
	FVeyraSpellSlotsTuning SpellSlots;
};

/** The Flux domain's checks that a schema cannot express, and its rules. */
namespace VeyraFlux
{
	/** Problems with Tuning, each a JSON pointer and a message; empty when it is consistent. */
	VEYRAFLUX_API TArray<FString> Validate(const FVeyraFluxTuning& Tuning);

	/** How many Flux Spell slots Permanent Team Flux unlocks: the thresholds it has reached (§14). */
	VEYRAFLUX_API int32 UnlockedSpellSlots(double Permanent, TConstArrayView<double> Thresholds);
}
