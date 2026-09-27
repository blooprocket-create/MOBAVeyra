// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Join/VeyraMatchRoster.h"

FVeyraMatchRoster::FVeyraMatchRoster(FVeyraMatchAssignment InAssignment)
	: Assignment(MoveTemp(InAssignment))
{
	Presence.SetNum(Assignment.Participants.Num());
}

const FVeyraAssignedParticipant* FVeyraMatchRoster::FindByTicketHash(FStringView TicketHash) const
{
	return Assignment.Participants.FindByPredicate([TicketHash](const FVeyraAssignedParticipant& Participant)
	{
		return FStringView(Participant.TicketHash).Equals(TicketHash, ESearchCase::CaseSensitive);
	});
}

const FVeyraAssignedParticipant* FVeyraMatchRoster::FindByAccount(FStringView AccountId) const
{
	return Assignment.Participants.FindByPredicate([AccountId](const FVeyraAssignedParticipant& Participant)
	{
		return FStringView(Participant.AccountId).Equals(AccountId, ESearchCase::CaseSensitive);
	});
}

FVeyraMatchRoster::FPresence* FVeyraMatchRoster::FindPresence(FStringView AccountId)
{
	const FVeyraAssignedParticipant* Participant = FindByAccount(AccountId);
	return Participant ? &Presence[static_cast<int32>(Participant - Assignment.Participants.GetData())] : nullptr;
}

const FVeyraMatchRoster::FPresence* FVeyraMatchRoster::FindPresence(FStringView AccountId) const
{
	return const_cast<FVeyraMatchRoster*>(this)->FindPresence(AccountId);
}

bool FVeyraMatchRoster::HasJoined(FStringView AccountId) const
{
	const FPresence* Found = FindPresence(AccountId);
	return Found && Found->bJoined;
}

bool FVeyraMatchRoster::IsConnected(FStringView AccountId) const
{
	const FPresence* Found = FindPresence(AccountId);
	return Found && Found->bConnected;
}

int32 FVeyraMatchRoster::NumConnected() const
{
	int32 Count = 0;
	for (const FPresence& Entry : Presence)
	{
		Count += Entry.bConnected ? 1 : 0;
	}
	return Count;
}

void FVeyraMatchRoster::MarkConnected(FStringView AccountId)
{
	if (FPresence* Found = FindPresence(AccountId))
	{
		Found->bJoined = true;
		Found->bConnected = true;
	}
}

void FVeyraMatchRoster::MarkDisconnected(FStringView AccountId)
{
	if (FPresence* Found = FindPresence(AccountId))
	{
		Found->bConnected = false;
	}
}

TArray<FVeyraParticipantResult> FVeyraMatchRoster::BuildParticipantResults() const
{
	TArray<FVeyraParticipantResult> Results;
	for (int32 Index = 0; Index < Assignment.Participants.Num(); ++Index)
	{
		Results.Add({ Assignment.Participants[Index].AccountId, Presence[Index].bJoined, Presence[Index].bConnected });
	}
	return Results;
}
