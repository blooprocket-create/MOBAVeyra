// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Join/VeyraMatchAssignment.h"

/**
 * The server's view of its rostered participants during one match: who may join, who has, and who
 * is connected now (ADR-007 §4). It holds no engine state, so the join rules are tested without a
 * world.
 */
class VEYRAMATCH_API FVeyraMatchRoster
{
public:
	explicit FVeyraMatchRoster(FVeyraMatchAssignment InAssignment);

	const FVeyraMatchAssignment& GetAssignment() const { return Assignment; }
	int32 Num() const { return Assignment.Participants.Num(); }

	/** The participant whose ticket has this hash, or null. */
	const FVeyraAssignedParticipant* FindByTicketHash(FStringView TicketHash) const;
	const FVeyraAssignedParticipant* FindByAccount(FStringView AccountId) const;

	/** Whether the account has joined at any time during this match. */
	bool HasJoined(FStringView AccountId) const;
	/** Whether the account is connected now. */
	bool IsConnected(FStringView AccountId) const;
	int32 NumConnected() const;

	void MarkConnected(FStringView AccountId);
	void MarkDisconnected(FStringView AccountId);

	/** Every rostered participant's result, in roster order. */
	TArray<FVeyraParticipantResult> BuildParticipantResults() const;

private:
	struct FPresence
	{
		bool bJoined = false;
		bool bConnected = false;
	};

	FPresence* FindPresence(FStringView AccountId);
	const FPresence* FindPresence(FStringView AccountId) const;

	FVeyraMatchAssignment Assignment;
	/** Parallel to Assignment.Participants. */
	TArray<FPresence> Presence;
};
