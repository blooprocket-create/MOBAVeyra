// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "Math/RandomStream.h"
#include "Misc/Optional.h"

#include "VeyraBotWanderComponent.generated.h"

/**
 * A practice bot's behaviour (Custom Matches Bible §1; ADR-010 §7): while the match is live, it walks
 * its Vanguard to a random point near the middle of the map every so often (Match.json bots). It does
 * not fight back. It sits on the bot's AVeyraVanguardController, on the server.
 */
UCLASS(ClassGroup = Match)
class VEYRAMATCH_API UVeyraBotWanderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVeyraBotWanderComponent();

	/** Seeds where the bot chooses to walk, so a match's bots are reproducible. */
	void SetSeed(int32 Seed) { Random.Initialize(Seed); }

	/**
	 * Orders the bot's Vanguard toward a random point within bots.wanderRadius of the point midway
	 * between the two sides' starts, and returns that point. Does nothing, returning nothing, unless
	 * the match is live and the bot has a Vanguard. Its timer calls it; tests call it directly.
	 */
	TOptional<FVector> Wander();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	FRandomStream Random;
	FTimerHandle Timer;
};
