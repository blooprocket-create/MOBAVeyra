// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "GameFramework/GameStateBase.h"

#include "VeyraTestClockGameState.generated.h"

// A game state for the Veyra.Abilities tests whose estimate of the server's clock can be offset from
// the local world clock, as a client's is. UHT forbids preprocessor guards around UCLASSes, so this
// header is unconditional.
UCLASS(NotBlueprintable, Transient)
class AVeyraTestClockGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	void SetServerClockOffset(float Seconds) { ServerWorldTimeSecondsDelta = Seconds; }
};
