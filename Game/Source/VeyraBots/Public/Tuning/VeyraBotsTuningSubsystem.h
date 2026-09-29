// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Hash/Blake3.h"
#include "Misc/Optional.h"
#include "Subsystems/EngineSubsystem.h"
#include "Tuning/VeyraBotsTuning.h"
#include "Tuning/VeyraTuning.h"
#include "VeyraMatchTypes.h"

#include "VeyraBotsTuningSubsystem.generated.h"

/**
 * Owns the Bots domain's tuning: loads and validates Game/Tuning/Bots.json once at startup (ADR-006
 * §6, ADR-013 §5), with the checks the schema cannot express (VeyraBots::Validate). Bots code reads it
 * through Get(); nothing else keeps a copy.
 */
UCLASS()
class VEYRABOTS_API UVeyraBotsTuningSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	/** The domain's file name in Game/Tuning. */
	static constexpr const TCHAR* Domain = TEXT("Bots");

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** The loaded Bots tuning. Fails a check if it did not load: bots never play on defaults. */
	static const FVeyraBotsTuning& Get();

	/** The behaviour of Difficulty. */
	static const FVeyraBotDifficultyTuning& GetDifficulty(EVeyraBotDifficulty Difficulty);

	/** How bots play Vanguard, or null if the file says nothing of it. */
	static const FVeyraBotVanguardTuning* FindVanguard(const FVeyraContentId& Vanguard);

	/** Reads and validates the file again, replacing the loaded tuning only when it is valid. */
	VeyraTuning::FErrors Reload();

	bool IsLoaded() const { return Tuning.IsSet(); }

	/** BLAKE3 of Bots.json's exact bytes, for comparing tuning between builds. */
	const FBlake3Hash& GetDocumentHash() const { return DocumentHash; }

#if WITH_DEV_AUTOMATION_TESTS
	/** Tests only: Get() returns this until it is cleared with nullptr. The caller keeps it alive. */
	static void SetTestOverride(const FVeyraBotsTuning* Override);
#endif

private:
	TOptional<FVeyraBotsTuning> Tuning;
	FBlake3Hash DocumentHash;
};
