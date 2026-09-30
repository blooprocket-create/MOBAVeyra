// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Absence/VeyraAbsenceRules.h"
#include "Statistics/VeyraMatchStatistics.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "VeyraAbsenceSubsystem.generated.h"

class AVeyraPlayerState;

/**
 * Server: the presence of each human participant in a live match (Match Flow Bible §4–§6; ADR-019
 * §2–§3). The game mode reports disconnects, returns and accepted orders; this owner turns them into
 * AFK warnings, personal losses and the autopilot, which walks an absent Vanguard behind the nearest
 * standing allied tower and then home, and only moves it. It runs on the match clock and, like the
 * world, stops while the match is paused. Bots are never absent.
 */
UCLASS()
class VEYRAMATCH_API UVeyraAbsenceSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Server: the match went live; each human participant's clock starts now. */
	void Start();

	/** Server: the match ended; nothing more changes. */
	void Stop();

	/** Server: Participant joined the match, or its Vanguard spawned; a bot is ignored. */
	void Track(AVeyraPlayerState& Participant);

	void NoteDisconnected(const AVeyraPlayerState& Participant);
	void NoteReturned(const AVeyraPlayerState& Participant);

	/** Server: Participant gave an accepted order; MoveDestination for a move. */
	void NoteActivity(const AVeyraPlayerState& Participant, const TOptional<FVector>& MoveDestination = {});

	/** The participant's record, or null if it is not tracked. */
	const FVeyraAbsenceRecord* Find(const AVeyraPlayerState& Participant) const;

	/** A participant's own outcome besides its team's. */
	struct FPersonalResult
	{
		bool bPersonalLoss = false;
		double AbsentSeconds = 0.0;
	};

	/**
	 * Server, at the end of a match of ActiveSeconds that Participant's team won or not: a personal loss
	 * unless a win forgives it, and its total absence (Match Flow Bible §6; ADR-019 §3). A contribution
	 * after its last return is a takedown, an assist, or damage, shielding, healing or a Well its record
	 * gained since. An untracked participant has neither.
	 */
	FPersonalResult Adjudicate(const AVeyraPlayerState& Participant, bool bTeamWon, double ActiveSeconds) const;

	/** Called when a participant becomes AFK or triggers a personal loss. */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAbsenceChanged, AVeyraPlayerState& /*Participant*/, const FVeyraAbsenceRecord& /*Record*/);
	FOnAbsenceChanged OnBecameAfk;
	FOnAbsenceChanged OnPersonalLoss;

	virtual void Tick(float DeltaSeconds) override;
	virtual bool IsTickable() const override { return bRunning; }
	virtual TStatId GetStatId() const override;

private:
	double Now() const;
	void Drive(AVeyraPlayerState& Participant, FVeyraAbsenceRecord& Record);

	struct FTracked
	{
		FVeyraAbsenceRecord Record;
		/** The stage the autopilot last walked toward, and the destination it chose. */
		EVeyraAutopilotStage Stage = EVeyraAutopilotStage::None;
		TOptional<FVector> Heading;
		/** Its record as it first came back after the absence that cost it, to tell what it did after. */
		TOptional<FVeyraPlayerStatistics> AtReturn;
	};

	/** Notes what Participant's record holds as it first comes back after a personal loss. */
	void NoteComeBack(const AVeyraPlayerState& Participant, FTracked& Entry) const;

	TMap<TWeakObjectPtr<AVeyraPlayerState>, FTracked> Tracked;
	bool bRunning = false;
};
