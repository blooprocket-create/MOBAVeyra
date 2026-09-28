// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Hash/Blake3.h"
#include "Misc/Optional.h"
#include "Subsystems/EngineSubsystem.h"
#include "Tuning/VeyraTuning.h"
#include "Tuning/VeyraWorldTuning.h"

#include "VeyraWorldTuningSubsystem.generated.h"

/**
 * Owns the World domain's tuning: loads and validates Game/Tuning/World.json once at startup
 * (ADR-006 §6), with the checks the schema cannot express (VeyraWorld::Validate). World code and the
 * map commandlet read it through Get(); nothing else keeps a copy.
 */
UCLASS()
class VEYRAWORLD_API UVeyraWorldTuningSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	/** The domain's file name in Game/Tuning. */
	static constexpr const TCHAR* Domain = TEXT("World");

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** The loaded World tuning. Fails a check if it did not load: the battleground never runs on defaults. */
	static const FVeyraWorldTuning& Get();

	/** Reads and validates the file again, replacing the loaded tuning only when it is valid. */
	VeyraTuning::FErrors Reload();

	bool IsLoaded() const { return Tuning.IsSet(); }

	/** BLAKE3 of World.json's exact bytes, for comparing tuning between builds. */
	const FBlake3Hash& GetDocumentHash() const { return DocumentHash; }

#if WITH_DEV_AUTOMATION_TESTS
	/** Tests only: Get() returns this until it is cleared with nullptr. The caller keeps it alive. */
	static void SetTestOverride(const FVeyraWorldTuning* Override);
#endif

private:
	TOptional<FVeyraWorldTuning> Tuning;
	FBlake3Hash DocumentHash;
};
