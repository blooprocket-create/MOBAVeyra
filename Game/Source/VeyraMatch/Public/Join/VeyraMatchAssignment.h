// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Teams/VeyraTeam.h"

/**
 * One rostered participant: who may join the match this server hosts, and on which side
 * (ADR-007 §5). The server never learns the participant's join ticket, only its hash.
 */
struct FVeyraAssignedParticipant
{
	FString AccountId;
	FString DisplayName;
	EVeyraTeam Side = EVeyraTeam::None;
	/** The lowercase hex SHA-256 of the participant's join ticket. */
	FString TicketHash;
};

/** The match a server hosts, as the backend assigned it. */
struct FVeyraMatchAssignment
{
	FString MatchId;
	TArray<FVeyraAssignedParticipant> Participants;
};

/** Why a match ended (ADR-007 §7–8). */
enum class EVeyraMatchEndReason : uint8
{
	/** A developer ended it; Shipping builds refuse this. */
	DeveloperRequest,
	/** No participant was connected for the tuned grace period. */
	Abandoned,
};

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
	/** None when no side won. No victory condition exists yet. */
	EVeyraTeam Winner = EVeyraTeam::None;
	/** The match clock, which excludes pauses; 0 if the match never went live. */
	double DurationSeconds = 0.0;
	TArray<FVeyraParticipantResult> Participants;
};
