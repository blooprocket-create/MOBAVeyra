// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"

#include "VeyraRegenerationComponent.generated.h"

/**
 * Restores a unit's resource and Health over time at its Resource Regeneration and Health
 * Regeneration stats, in and out of combat (Combat Bible §6, §28; ADR-011 §11). It sits beside the
 * unit's Ability System Component and runs on the server on a world-time timer, so it freezes with
 * the match during a pause (ADR-006 §8).
 */
UCLASS()
class VEYRACOMBAT_API UVeyraRegenerationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVeyraRegenerationComponent();

	/**
	 * Server only: restores what TickSeconds of regeneration gives. The timer calls it every
	 * Combat.json regeneration.tickSeconds; tests call it directly. A dead unit regenerates nothing.
	 */
	void ApplyTick(double TickSeconds);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void OnTimer();

	FTimerHandle Timer;
	double TickSeconds = 0.0;
};
