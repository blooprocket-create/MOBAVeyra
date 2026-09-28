// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Hash/Blake3.h"
#include "Misc/Optional.h"
#include "Rewards/VeyraEconomyTuning.h"
#include "Subsystems/EngineSubsystem.h"
#include "Tuning/VeyraTuning.h"

#include "VeyraEconomyTuningSubsystem.generated.h"

/**
 * Owns the Economy domain's reward tuning: loads and validates Game/Tuning/Economy.json once at
 * startup (ADR-006 §6), with the checks the schema cannot express (VeyraRewards::Validate). Reward
 * code reads it through Get(); nothing else keeps a copy.
 */
UCLASS()
class VEYRAECONOMY_API UVeyraEconomyTuningSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	/** The domain's file name in Game/Tuning. */
	static constexpr const TCHAR* Domain = TEXT("Economy");

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** The loaded Economy tuning. Fails a check if it did not load: rewards never run on defaults. */
	static const FVeyraEconomyTuning& Get();

	/** Reads and validates the file again, replacing the loaded tuning only when it is valid. */
	VeyraTuning::FErrors Reload();

	bool IsLoaded() const { return Tuning.IsSet(); }

	/** BLAKE3 of Economy.json's exact bytes, for comparing tuning between builds. */
	const FBlake3Hash& GetDocumentHash() const { return DocumentHash; }

#if WITH_DEV_AUTOMATION_TESTS
	/** Tests only: Get() returns this until it is cleared with nullptr. The caller keeps it alive. */
	static void SetTestOverride(const FVeyraEconomyTuning* Override);
#endif

private:
	TOptional<FVeyraEconomyTuning> Tuning;
	FBlake3Hash DocumentHash;
};
