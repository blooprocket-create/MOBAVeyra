// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Hash/Blake3.h"
#include "Misc/Optional.h"
#include "Subsystems/EngineSubsystem.h"
#include "Tuning/VeyraItemsTuning.h"
#include "Tuning/VeyraTuning.h"

#include "VeyraItemsTuningSubsystem.generated.h"

/**
 * Owns the Items domain's tuning: loads and validates Game/Tuning/Items.json once at startup
 * (ADR-006 §6), with the checks the schema cannot express (VeyraItems::Validate). Items code reads it
 * through Get(); nothing else keeps a copy.
 */
UCLASS()
class VEYRAITEMS_API UVeyraItemsTuningSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	/** The domain's file name in Game/Tuning. */
	static constexpr const TCHAR* Domain = TEXT("Items");

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** The loaded Items tuning. Fails a check if it did not load: the shop never runs on defaults. */
	static const FVeyraItemsTuning& Get();

	/** The item Id defines, or null. */
	static const FVeyraItemDefinition* FindItem(const FVeyraContentId& Id);

	/** Reads and validates the file again, replacing the loaded tuning only when it is valid. */
	VeyraTuning::FErrors Reload();

	bool IsLoaded() const { return Tuning.IsSet(); }

	/** BLAKE3 of Items.json's exact bytes, for comparing tuning between builds. */
	const FBlake3Hash& GetDocumentHash() const { return DocumentHash; }

#if WITH_DEV_AUTOMATION_TESTS
	/** Tests only: Get() returns this until it is cleared with nullptr. The caller keeps it alive. */
	static void SetTestOverride(const FVeyraItemsTuning* Override);
#endif

private:
	TOptional<FVeyraItemsTuning> Tuning;
	FBlake3Hash DocumentHash;
};
