// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Hash/Blake3.h"
#include "Misc/Optional.h"
#include "Subsystems/EngineSubsystem.h"
#include "Tuning/VeyraCombatTuning.h"
#include "Tuning/VeyraTuning.h"

#include "VeyraCombatTuningSubsystem.generated.h"

/**
 * Owns the Combat domain's tuning: loads and validates Game/Tuning/Combat.json once at startup
 * (ADR-006 §6). Combat code reads it through Get(); nothing else keeps a copy.
 */
UCLASS()
class VEYRACOMBAT_API UVeyraCombatTuningSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	/** The domain's file name in Game/Tuning. */
	static constexpr const TCHAR* Domain = TEXT("Combat");

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** The loaded Combat tuning. Fails a check if it did not load: combat never runs on defaults. */
	static const FVeyraCombatTuning& Get();

	/** Reads and validates the file again, replacing the loaded tuning only when it is valid. */
	VeyraTuning::FErrors Reload();

	bool IsLoaded() const { return Tuning.IsSet(); }

	/** BLAKE3 of Combat.json's exact bytes, for comparing tuning between builds. */
	const FBlake3Hash& GetDocumentHash() const { return DocumentHash; }

private:
	TOptional<FVeyraCombatTuning> Tuning;
	FBlake3Hash DocumentHash;
};
