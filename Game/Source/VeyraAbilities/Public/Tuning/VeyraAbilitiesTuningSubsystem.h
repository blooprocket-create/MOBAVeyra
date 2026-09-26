// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Hash/Blake3.h"
#include "Misc/Optional.h"
#include "Subsystems/EngineSubsystem.h"
#include "Tuning/VeyraAbilitiesTuning.h"
#include "Tuning/VeyraTuning.h"

#include "VeyraAbilitiesTuningSubsystem.generated.h"

/**
 * Owns the Abilities domain's tuning: loads and validates Game/Tuning/Abilities.json once at startup
 * (ADR-006 §6). Ability code reads it through Get(); nothing else keeps a copy.
 */
UCLASS()
class VEYRAABILITIES_API UVeyraAbilitiesTuningSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	/** The domain's file name in Game/Tuning. */
	static constexpr const TCHAR* Domain = TEXT("Abilities");

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** The loaded Abilities tuning. Fails a check if it did not load: abilities never run on defaults. */
	static const FVeyraAbilitiesTuning& Get();

	/** A targeted damage ability's tuning, or null if no such ability exists. */
	static const FVeyraTargetedDamageAbilityTuning* FindTargetedDamage(const FVeyraContentId& Ability);

	/** Whether any archetype defines Ability. Other domains check references against it. */
	static bool Defines(const FVeyraContentId& Ability);

	/** Reads and validates the file again, replacing the loaded tuning only when it is valid. */
	VeyraTuning::FErrors Reload();

	bool IsLoaded() const { return Tuning.IsSet(); }

	/** BLAKE3 of Abilities.json's exact bytes, for comparing tuning between builds. */
	const FBlake3Hash& GetDocumentHash() const { return DocumentHash; }

#if WITH_DEV_AUTOMATION_TESTS
	/** Tests only: Get() returns this until it is cleared with nullptr. The caller keeps it alive. */
	static void SetTestOverride(const FVeyraAbilitiesTuning* Override);
#endif

private:
	TOptional<FVeyraAbilitiesTuning> Tuning;
	FBlake3Hash DocumentHash;
};
