// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "GameFramework/GameStateBase.h"
#include "VeyraMatchTypes.h"

#include "VeyraGameState.generated.h"

/** The match state every client sees: the phase, the pause and the match clock (Match Flow Bible §1). */
UCLASS()
class VEYRAMATCH_API AVeyraGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	EVeyraMatchPhase GetPhase() const { return Phase; }

	/** Whether an approved pause holds the match (Match Flow Bible §1, stage 5, and §10). */
	bool IsMatchPaused() const { return bMatchPaused; }

	/**
	 * The server's gameplay time as this machine knows it. It stands still while the match is paused
	 * (Match Flow Bible §10.2), so every clock a client shows from it stops too.
	 */
	double GetGameplayServerTime() const;

	/** Seconds since the match went live, 0 before that. */
	double GetMatchClockSeconds() const;

	/** Server only: the GameMode advances the phase. */
	void SetPhase(EVeyraMatchPhase NewPhase);

	/** Server only: the GameMode pauses and resumes the match. */
	void SetMatchPaused(bool bPaused);

private:
	UPROPERTY(Replicated)
	EVeyraMatchPhase Phase = EVeyraMatchPhase::Loading;

	/** Server gameplay time when the match went live. */
	UPROPERTY(Replicated)
	double LiveStartServerTime = 0.0;

	UPROPERTY(Replicated)
	bool bMatchPaused = false;

	/** Server gameplay time when the pause began. */
	UPROPERTY(Replicated)
	double PausedAtServerTime = 0.0;
};
