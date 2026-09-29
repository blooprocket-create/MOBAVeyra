// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Hash/Blake3.h"
#include "Misc/Optional.h"
#include "Subsystems/EngineSubsystem.h"
#include "Tuning/VeyraFluxTuning.h"
#include "Tuning/VeyraTuning.h"

#include "VeyraFluxTuningSubsystem.generated.h"

/**
 * Owns the Flux domain's tuning: loads and validates Game/Tuning/Flux.json once at startup
 * (ADR-006 §6), with the checks the schema cannot express (VeyraFlux::Validate). Flux code reads it
 * through Get(); nothing else keeps a copy.
 */
UCLASS()
class VEYRAFLUX_API UVeyraFluxTuningSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	/** The domain's file name in Game/Tuning. */
	static constexpr const TCHAR* Domain = TEXT("Flux");

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** The loaded Flux tuning. Fails a check if it did not load: Team Flux never runs on defaults. */
	static const FVeyraFluxTuning& Get();

	/** Reads and validates the file again, replacing the loaded tuning only when it is valid. */
	VeyraTuning::FErrors Reload();

	bool IsLoaded() const { return Tuning.IsSet(); }

	/** BLAKE3 of Flux.json's exact bytes, for comparing tuning between builds. */
	const FBlake3Hash& GetDocumentHash() const { return DocumentHash; }

#if WITH_DEV_AUTOMATION_TESTS
	/** Tests only: Get() returns this until it is cleared with nullptr. The caller keeps it alive. */
	static void SetTestOverride(const FVeyraFluxTuning* Override);
#endif

private:
	TOptional<FVeyraFluxTuning> Tuning;
	FBlake3Hash DocumentHash;
};
