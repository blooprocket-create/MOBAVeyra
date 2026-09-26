// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Hash/Blake3.h"
#include "Misc/Optional.h"
#include "Subsystems/EngineSubsystem.h"
#include "Tuning/VeyraMatchTuning.h"
#include "Tuning/VeyraTuning.h"

#include "VeyraMatchTuningSubsystem.generated.h"

/**
 * Owns the Match domain's tuning: loads and validates Game/Tuning/Match.json once at startup
 * (ADR-006 §6). Match code reads it through Get(); nothing else keeps a copy.
 */
UCLASS()
class VEYRAMATCH_API UVeyraMatchTuningSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	/** The domain's file name in Game/Tuning. */
	static constexpr const TCHAR* Domain = TEXT("Match");

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** The loaded Match tuning. Fails a check if it did not load: a match never runs on defaults. */
	static const FVeyraMatchTuning& Get();

	/** Reads and validates the file again, replacing the loaded tuning only when it is valid. */
	VeyraTuning::FErrors Reload();

	bool IsLoaded() const { return Tuning.IsSet(); }

	/** BLAKE3 of Match.json's exact bytes, for comparing tuning between builds. */
	const FBlake3Hash& GetDocumentHash() const { return DocumentHash; }

#if WITH_DEV_AUTOMATION_TESTS
	/**
	 * Tests only: Get() returns this instead of the loaded file until it is cleared with nullptr.
	 * The caller keeps the struct alive. It does not change the hash reported to the server.
	 */
	static void SetTestOverride(const FVeyraMatchTuning* Override);
#endif

private:
	TOptional<FVeyraMatchTuning> Tuning;
	FBlake3Hash DocumentHash;
};
