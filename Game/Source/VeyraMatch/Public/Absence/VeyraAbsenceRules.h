// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Math/Vector.h"
#include "Misc/Optional.h"

struct FVeyraAbsenceTuning;
struct FVeyraActivityTuning;
struct FVeyraAutopilotTuning;

/** Whether a participant is playing, AFK or disconnected (Match Flow Bible §4–§5). */
enum class EVeyraAbsence : uint8
{
	Present,
	/** Connected, but without meaningful activity for the tuned time. */
	Afk,
	Disconnected,
};

/** Where the autopilot takes an absent Vanguard now (Match Flow Bible §4; ADR-019 §2). */
enum class EVeyraAutopilotStage : uint8
{
	None,
	/** Behind the nearest standing allied tower. */
	BehindTower,
	/** Home, where it stays. */
	Fountain,
};

/**
 * One human participant's presence in a live match (ADR-019 §3). Every time is in match seconds, so a
 * pause stops every clock.
 */
struct FVeyraAbsenceRecord
{
	bool bConnected = true;
	EVeyraAbsence Absence = EVeyraAbsence::Present;
	/** When it last did something meaningful, came back, or the match went live. */
	double LastActiveAt = 0.0;
	/** When the absence it is in began; unset while present. */
	TOptional<double> AwaySince;
	/** When the continuous trigger for a personal loss began: the AFK warning, or the disconnect. */
	TOptional<double> TriggerSince;
	/** Absence already over, in seconds. */
	double ClosedAbsentSeconds = 0.0;
	bool bPersonalLoss = false;
	/** When it last came back from an absence. */
	TOptional<double> ReturnedAt;
	/** Where the last move counted as activity was going. */
	TOptional<FVector> LastCountedMove;
};

/** The pure rules of absence, AFK, personal loss and the autopilot (Match Flow Bible §4–§6; ADR-019 §2–§3). */
namespace VeyraAbsence
{
	/** What an update found newly true. */
	struct FChange
	{
		bool bBecameAfk = false;
		bool bPenalized = false;
	};

	/** Moves Record to Now: an inactive participant becomes AFK, and a long absence becomes a personal loss. */
	VEYRAMATCH_API FChange Update(FVeyraAbsenceRecord& Record, double Now, const FVeyraAbsenceTuning& Tuning);

	/** It disconnected at Now; an AFK participant that disconnects starts the disconnect trigger afresh. */
	VEYRAMATCH_API void NoteDisconnected(FVeyraAbsenceRecord& Record, double Now);

	/** It connected again at Now: its absence ends and its continuous trigger resets; the total stays. */
	VEYRAMATCH_API void NoteConnected(FVeyraAbsenceRecord& Record, double Now);

	/**
	 * It gave an accepted order at Now, with MoveDestination for a move. Returns whether that counts as
	 * meaningful activity: any other order does, and a move whose destination lies beyond the tuned
	 * distance of the last move counted. Activity ends an AFK absence.
	 */
	VEYRAMATCH_API bool NoteActivity(FVeyraAbsenceRecord& Record, double Now, const TOptional<FVector>& MoveDestination, const FVeyraActivityTuning& Tuning);

	/** Every second of absence up to Now: those over, and the one it is in. */
	VEYRAMATCH_API double AbsentSeconds(const FVeyraAbsenceRecord& Record, double Now);

	/** Behind a tower for the tuned time after the absence began, then home; nothing while present. */
	VEYRAMATCH_API EVeyraAutopilotStage StageOf(const FVeyraAbsenceRecord& Record, double Now, const FVeyraAutopilotTuning& Tuning);

	/**
	 * Where the autopilot walks the Vanguard at From: behind the nearest of Towers, Distance from it
	 * toward Home, or Home itself for the fountain stage or when no allied tower stands.
	 */
	VEYRAMATCH_API FVector Destination(EVeyraAutopilotStage Stage, const FVector& From, TConstArrayView<FVector> Towers, const FVector& Home, double Distance);

	/**
	 * Whether a personal loss is forgiven at the end (Match Flow Bible §6): the team won, the total
	 * absence, counting any it is still in at the end, is within the tuned share of the active
	 * duration, and it contributed after it came back from the absence that cost it.
	 */
	VEYRAMATCH_API bool IsForgiven(const FVeyraAbsenceRecord& Record, bool bTeamWon, double ActiveSeconds, bool bContributedSinceReturn,
		const FVeyraAbsenceTuning& Tuning);
}
