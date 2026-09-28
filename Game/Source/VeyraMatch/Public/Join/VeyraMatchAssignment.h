// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "CoreMinimal.h"
#include "Teams/VeyraTeam.h"
#include "VeyraMatchTypes.h"

/**
 * One rostered participant: who may join the match this server hosts, on which side, and as which
 * Vanguard (ADR-007 §5, ADR-010 §9). The server never learns the participant's join ticket, only its
 * hash.
 */
struct FVeyraAssignedParticipant
{
	FString AccountId;
	FString DisplayName;
	EVeyraTeam Side = EVeyraTeam::None;
	/** The lowercase hex SHA-256 of the participant's join ticket. */
	FString TicketHash;
	/** The Vanguard the participant locked in champion select. */
	FVeyraContentId VanguardId;
};

/**
 * One AI participant the server adds when the match starts (Custom Matches Bible §1; ADR-010 §7). A
 * bot is no account: it has no ticket and no result.
 */
struct FVeyraAssignedBot
{
	EVeyraTeam Side = EVeyraTeam::None;
	FVeyraContentId VanguardId;
	EVeyraBotDifficulty Difficulty = EVeyraBotDifficulty::Beginner;
};

/** The match a server hosts, as the backend assigned it. */
struct FVeyraMatchAssignment
{
	FString MatchId;
	/** The mode the match records, such as casual_select or custom_practice. */
	FVeyraContentId Mode;
	EVeyraMatchRules Rules = EVeyraMatchRules::Standard;
	/** The account that hosts a practice match; empty for standard rules. */
	FString HostAccountId;
	TArray<FVeyraAssignedParticipant> Participants;
	/** Only a practice match has bots. */
	TArray<FVeyraAssignedBot> Bots;
};

/** Why a match ended (ADR-007 §7–8, ADR-010 §7, ADR-011 §13). */
enum class EVeyraMatchEndReason : uint8
{
	/** A developer ended it; Shipping builds refuse this. */
	DeveloperRequest,
	/** No participant was connected for the tuned grace period. */
	Abandoned,
	/** The host ended a practice match (Custom Matches Bible §4). */
	HostEnded,
	/** A side destroyed the other's Prime Well and won a standard match (Battleground Bible §18). The one end with a winner. */
	PrimeWellDestroyed,
};

namespace VeyraMatchResults
{
	/** Whether a match that ended for Reason names Winner rightly: a side exactly when a Prime Well fell. */
	VEYRAMATCH_API bool IsWinnerConsistent(EVeyraMatchEndReason Reason, EVeyraTeam Winner);
}

VEYRAMATCH_API const TCHAR* LexToString(EVeyraMatchEndReason Reason);

/** What the server reports about one rostered participant when the match ends. */
struct FVeyraParticipantResult
{
	FString AccountId;
	bool bJoined = false;
	bool bConnectedAtEnd = false;
};

/** How a match ended, as its server reports it (ADR-007 §7). */
struct FVeyraMatchResult
{
	/** Empty for a developer match that has no assignment. */
	FString MatchId;
	EVeyraMatchEndReason EndReason = EVeyraMatchEndReason::DeveloperRequest;
	/** The side that destroyed the other's Prime Well; None for every other end. */
	EVeyraTeam Winner = EVeyraTeam::None;
	/** The match clock, which excludes pauses; 0 if the match never went live. */
	double DurationSeconds = 0.0;
	TArray<FVeyraParticipantResult> Participants;
};
