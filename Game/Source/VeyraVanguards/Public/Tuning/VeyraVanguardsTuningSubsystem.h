// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Hash/Blake3.h"
#include "Misc/Optional.h"
#include "Subsystems/EngineSubsystem.h"
#include "Tuning/VeyraTuning.h"
#include "Tuning/VeyraVanguardsTuning.h"

#include "VeyraVanguardsTuningSubsystem.generated.h"

/**
 * Owns the Vanguards domain's tuning: loads and validates Game/Tuning/Vanguards.json once at startup
 * (ADR-006 §6), after the Abilities and Progression tuning it refers to. Nothing else keeps a copy.
 */
UCLASS()
class VEYRAVANGUARDS_API UVeyraVanguardsTuningSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	/** The domain's file name in Game/Tuning. */
	static constexpr const TCHAR* Domain = TEXT("Vanguards");

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** The loaded Vanguards tuning. Fails a check if it did not load. */
	static const FVeyraVanguardsTuning& Get();

	/** A Vanguard's definition, or null if no such Vanguard exists. */
	static const FVeyraVanguardDefinition* FindVanguard(const FVeyraContentId& Vanguard);

	/** A Deep Foundation passive's tuning, or null. */
	static const FVeyraDeepFoundationTuning* FindDeepFoundation(const FVeyraContentId& Passive);

	/** A hit-chain passive's tuning, or null. */
	static const FVeyraHitChainTuning* FindHitChain(const FVeyraContentId& Passive);

	/** Reads and validates the file again, replacing the loaded tuning only when it is valid. */
	VeyraTuning::FErrors Reload();

	bool IsLoaded() const { return Tuning.IsSet(); }

	/** BLAKE3 of Vanguards.json's exact bytes, for comparing tuning between builds. */
	const FBlake3Hash& GetDocumentHash() const { return DocumentHash; }

#if WITH_DEV_AUTOMATION_TESTS
	/** Tests only: Get() returns this until it is cleared with nullptr. The caller keeps it alive. */
	static void SetTestOverride(const FVeyraVanguardsTuning* Override);
#endif

private:
	TOptional<FVeyraVanguardsTuning> Tuning;
	FBlake3Hash DocumentHash;
};
