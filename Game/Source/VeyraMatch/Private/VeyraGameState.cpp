// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraGameState.h"

#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

void AVeyraGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraGameState, Phase, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraGameState, LiveStartServerTime, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraGameState, bMatchPaused, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraGameState, PausedAtServerTime, Params);
}

double AVeyraGameState::GetGameplayServerTime() const
{
	// The server's world stops while paused, but a client's world may still be catching up with the
	// pause, so a paused match reads the time the pause began.
	return bMatchPaused ? PausedAtServerTime : GetServerWorldTimeSeconds();
}

double AVeyraGameState::GetMatchClockSeconds() const
{
	return Phase == EVeyraMatchPhase::Live ? GetGameplayServerTime() - LiveStartServerTime : 0.0;
}

void AVeyraGameState::SetPhase(EVeyraMatchPhase NewPhase)
{
	check(HasAuthority());
	Phase = NewPhase;
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraGameState, Phase, this);
	if (NewPhase == EVeyraMatchPhase::Live)
	{
		LiveStartServerTime = GetGameplayServerTime();
		MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraGameState, LiveStartServerTime, this);
	}
}

void AVeyraGameState::SetMatchPaused(bool bPaused)
{
	check(HasAuthority());
	if (bPaused)
	{
		PausedAtServerTime = GetServerWorldTimeSeconds();
		MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraGameState, PausedAtServerTime, this);
	}
	bMatchPaused = bPaused;
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraGameState, bMatchPaused, this);
}
