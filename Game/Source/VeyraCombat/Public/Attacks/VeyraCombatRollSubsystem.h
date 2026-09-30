// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Math/RandomStream.h"
#include "Random/VeyraOutcomeBag.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ObjectKey.h"
#include "VeyraCombatRollSubsystem.generated.h"

class UAbilitySystemComponent;

/**
 * The match's random source for combat (ADR-022 §1, §10). Each unit's independent sources of chance,
 * such as its basic attacks' crits or an ability's own, draw from outcome bags of their own
 * (FVeyraOutcomeBag), each seeded from the match's stream when first used. The stream is seeded when
 * the world begins play and says its seed in the log, so a match's outcomes can be reproduced; tests
 * seed it themselves. Clients never roll: the server decides every outcome.
 */
UCLASS()
class VEYRACOMBAT_API UVeyraCombatRollSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/**
	 * The next value in [0, 1) from Unit's bag for Channel, a bag of Draws values. One channel's draws
	 * never change another's. A world without this subsystem draws an independent value.
	 */
	static double Draw(const UWorld* World, const UAbilitySystemComponent& Unit, const FVeyraContentId& Channel, int32 Draws);

	/** Starts the stream again from Seed, with every bag empty. */
	void SetSeed(int32 Seed);

	int32 GetSeed() const { return Stream.GetInitialSeed(); }

private:
	FRandomStream Stream;

	/** Each unit's bags, by channel. */
	TMap<TPair<FObjectKey, FVeyraContentId>, FVeyraOutcomeBag> Bags;
};
