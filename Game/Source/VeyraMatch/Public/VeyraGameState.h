// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "GameFramework/GameStateBase.h"
#include "VeyraMatchTypes.h"

#include "VeyraGameState.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnVeyraMatchPhaseChanged, EVeyraMatchPhase /*NewPhase*/);

/**
 * The match state every client sees: the phase, the pause, the match clock, and the rules with a
 * custom match's host (Match Flow Bible §1; ADR-010 §7).
 */
UCLASS()
class VEYRAMATCH_API AVeyraGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	EVeyraMatchPhase GetPhase() const { return Phase; }

	/** Broadcast on the server and on every client whenever the phase changes. */
	FOnVeyraMatchPhaseChanged OnPhaseChanged;

	EVeyraMatchRules GetMatchRules() const { return MatchRules; }

	/** The custom match's host, once they have joined; null for standard rules. */
	const APlayerState* GetHost() const { return Host; }

	/** Server only: the GameMode sets the assigned rules when the match starts. */
	void SetMatchRules(EVeyraMatchRules Rules);

	/** Server only: the GameMode names the custom match's host when they join. */
	void SetHost(APlayerState* InHost);

	/** Whether an approved pause holds the match (Match Flow Bible §1, stage 5, and §10). */
	bool IsMatchPaused() const { return bMatchPaused; }

	/**
	 * The server's gameplay time as this machine knows it. It stands still while the match is paused
	 * (Match Flow Bible §10.2), so every clock a client shows from it stops too.
	 */
	double GetGameplayServerTime() const;

	/** Seconds since the match went live: 0 before that, and frozen once it ends. */
	double GetMatchClockSeconds() const;

	/** Server only: the GameMode advances the phase. */
	void SetPhase(EVeyraMatchPhase NewPhase);

	/** Server only: the GameMode pauses and resumes the match. */
	void SetMatchPaused(bool bPaused);

private:
	UFUNCTION()
	void OnRep_Phase();

	UPROPERTY(ReplicatedUsing = OnRep_Phase)
	EVeyraMatchPhase Phase = EVeyraMatchPhase::Loading;

	UPROPERTY(Replicated)
	EVeyraMatchRules MatchRules = EVeyraMatchRules::Standard;

	UPROPERTY(Replicated)
	TObjectPtr<APlayerState> Host;

	/** Server gameplay time when the match went live. */
	UPROPERTY(Replicated)
	double LiveStartServerTime = 0.0;

	/** The match clock when the match ended; 0 if it ended before going live. */
	UPROPERTY(Replicated)
	double MatchClockAtEnd = 0.0;

	UPROPERTY(Replicated)
	bool bMatchPaused = false;

	/** Server gameplay time when the pause began. */
	UPROPERTY(Replicated)
	double PausedAtServerTime = 0.0;
};
