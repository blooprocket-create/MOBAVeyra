// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Math/RandomStream.h"
#include "Subsystems/WorldSubsystem.h"

#include "VeyraCombatRollSubsystem.generated.h"

/**
 * The match's one random source for combat (ADR-022 §1): a stream the server draws crit rolls from.
 * It is seeded when the world begins play and says its seed in the log, so a match's rolls can be
 * reproduced; tests seed it themselves. Clients never roll: the server decides every outcome.
 */
UCLASS()
class VEYRACOMBAT_API UVeyraCombatRollSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** A uniform draw in [0, 1) from World's stream; a world without one draws from the engine's. */
	static double Roll(const UWorld* World);

	/** Starts the stream again from Seed. */
	void SetSeed(int32 Seed);
	int32 GetSeed() const { return Stream.GetInitialSeed(); }

private:
	FRandomStream Stream;
};
