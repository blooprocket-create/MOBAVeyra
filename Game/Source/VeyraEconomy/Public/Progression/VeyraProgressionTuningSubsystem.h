// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Hash/Blake3.h"
#include "Misc/Optional.h"
#include "Progression/VeyraProgressionTuning.h"
#include "Subsystems/EngineSubsystem.h"
#include "Tuning/VeyraTuning.h"

#include "VeyraProgressionTuningSubsystem.generated.h"

/**
 * Owns the Progression domain's tuning: loads and validates Game/Tuning/Progression.json once at
 * startup (ADR-006 §6), with the checks the schema cannot express (VeyraProgression::Validate).
 * Progression code reads it through Get(); nothing else keeps a copy.
 */
UCLASS()
class VEYRAECONOMY_API UVeyraProgressionTuningSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	/** The domain's file name in Game/Tuning. */
	static constexpr const TCHAR* Domain = TEXT("Progression");

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** The loaded Progression tuning. Fails a check if it did not load: progression never runs on defaults. */
	static const FVeyraProgressionTuning& Get();

	/** Reads and validates the file again, replacing the loaded tuning only when it is valid. */
	VeyraTuning::FErrors Reload();

	bool IsLoaded() const { return Tuning.IsSet(); }

	/** BLAKE3 of Progression.json's exact bytes, for comparing tuning between builds. */
	const FBlake3Hash& GetDocumentHash() const { return DocumentHash; }

#if WITH_DEV_AUTOMATION_TESTS
	/** Tests only: Get() returns this until it is cleared with nullptr. The caller keeps it alive. */
	static void SetTestOverride(const FVeyraProgressionTuning* Override);
#endif

private:
	TOptional<FVeyraProgressionTuning> Tuning;
	FBlake3Hash DocumentHash;
};
