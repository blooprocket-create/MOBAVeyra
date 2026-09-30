// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraGameState.h"

#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Rules/VeyraMatchRules.h"

void AVeyraGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraGameState, Phase, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraGameState, MatchRules, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraGameState, bVictoryEnabled, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraGameState, Host, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraGameState, LiveStartServerTime, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraGameState, MatchClockAtEnd, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraGameState, bMatchPaused, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraGameState, PausedAtServerTime, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraGameState, Vote, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraGameState, IntermissionSecondsLeft, Params);
}

void AVeyraGameState::SetVote(const FVeyraVoteState& InVote)
{
	if (Vote == InVote)
	{
		return;
	}
	Vote = InVote;
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraGameState, Vote, this);
}

void AVeyraGameState::SetIntermissionSecondsLeft(int32 Seconds)
{
	if (IntermissionSecondsLeft == Seconds)
	{
		return;
	}
	IntermissionSecondsLeft = Seconds;
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraGameState, IntermissionSecondsLeft, this);
}

double AVeyraGameState::GetGameplayServerTime() const
{
	// The server's world stops while paused, but a client's world may still be catching up with the
	// pause, so a paused match reads the time the pause began.
	return bMatchPaused ? PausedAtServerTime : GetServerWorldTimeSeconds();
}

double AVeyraGameState::GetMatchClockSeconds() const
{
	switch (Phase)
	{
	case EVeyraMatchPhase::Live:
		return GetGameplayServerTime() - LiveStartServerTime;
	case EVeyraMatchPhase::Ended:
		return MatchClockAtEnd;
	default:
		return 0.0;
	}
}

void AVeyraGameState::SetPhase(EVeyraMatchPhase NewPhase)
{
	check(HasAuthority());
	if (NewPhase == EVeyraMatchPhase::Ended)
	{
		// Read before the phase changes, while the clock still runs.
		MatchClockAtEnd = GetMatchClockSeconds();
		MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraGameState, MatchClockAtEnd, this);
	}
	Phase = NewPhase;
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraGameState, Phase, this);
	if (NewPhase == EVeyraMatchPhase::Live)
	{
		LiveStartServerTime = GetGameplayServerTime();
		MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraGameState, LiveStartServerTime, this);
	}
	OnPhaseChanged.Broadcast(Phase);
}

void AVeyraGameState::OnRep_Phase()
{
	OnPhaseChanged.Broadcast(Phase);
}

void AVeyraGameState::SetMatchRules(EVeyraMatchRules Rules, TOptional<bool> bInVictoryEnabled)
{
	check(HasAuthority());
	MatchRules = Rules;
	bVictoryEnabled = bInVictoryEnabled.Get(VeyraMatchRules::HasVictory(Rules, {}));
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraGameState, MatchRules, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraGameState, bVictoryEnabled, this);
}

void AVeyraGameState::SetHost(APlayerState* InHost)
{
	check(HasAuthority());
	Host = InHost;
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraGameState, Host, this);
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
