// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Hash/Blake3.h"
#include "Misc/Optional.h"
#include "Subsystems/EngineSubsystem.h"
#include "Tuning/VeyraVisionTuning.h"
#include "Tuning/VeyraTuning.h"

#include "VeyraVisionTuningSubsystem.generated.h"

/**
 * Owns the Vision domain's tuning: loads and validates Game/Tuning/Vision.json once at startup
 * (ADR-006 §6), with the checks the schema cannot express (VeyraVision::Validate). Vision code reads it
 * through Get(); nothing else keeps a copy.
 */
UCLASS()
class VEYRAVISION_API UVeyraVisionTuningSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	/** The domain's file name in Game/Tuning. */
	static constexpr const TCHAR* Domain = TEXT("Vision");

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** The loaded Vision tuning. Fails a check if it did not load: vision never runs on defaults. */
	static const FVeyraVisionTuning& Get();

	/** Reads and validates the file again, replacing the loaded tuning only when it is valid. */
	VeyraTuning::FErrors Reload();

	bool IsLoaded() const { return Tuning.IsSet(); }

	/** BLAKE3 of Vision.json's exact bytes, for comparing tuning between builds. */
	const FBlake3Hash& GetDocumentHash() const { return DocumentHash; }

#if WITH_DEV_AUTOMATION_TESTS
	/** Tests only: Get() returns this until it is cleared with nullptr. The caller keeps it alive. */
	static void SetTestOverride(const FVeyraVisionTuning* Override);
#endif

private:
	TOptional<FVeyraVisionTuning> Tuning;
	FBlake3Hash DocumentHash;
};
