// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Join/VeyraMatchHostSubsystem.h"

#include "Engine/Engine.h"
#include "Tuning/VeyraMatchTuningSubsystem.h"
#include "VeyraMatchLog.h"

namespace
{
	/** A SHA-256 in lowercase hex: 64 characters. A format fact, not tuning. */
	constexpr int32 HostTicketHashLength = 64;

	bool IsLowercaseHexHash(const FString& Text)
	{
		if (Text.Len() != HostTicketHashLength)
		{
			return false;
		}
		for (const TCHAR Character : Text)
		{
			if (!FChar::IsDigit(Character) && !(Character >= TEXT('a') && Character <= TEXT('f')))
			{
				return false;
			}
		}
		return true;
	}
}

UVeyraMatchHostSubsystem* UVeyraMatchHostSubsystem::Get()
{
	return GEngine ? GEngine->GetEngineSubsystem<UVeyraMatchHostSubsystem>() : nullptr;
}

TArray<FString> UVeyraMatchHostSubsystem::SetAssignment(FVeyraMatchAssignment InAssignment)
{
	TArray<FString> Problems;
	if (InAssignment.MatchId.IsEmpty())
	{
		Problems.Add(TEXT("the assignment has no match ID"));
	}
	if (InAssignment.Participants.IsEmpty())
	{
		Problems.Add(TEXT("the assignment has no participants"));
	}

	const int32 MaxTeamSize = UVeyraMatchTuningSubsystem::Get().Teams.MaxTeamSize;
	TSet<FString> Accounts;
	TSet<FString> Hashes;
	int32 SideCounts[2] = { 0, 0 };
	for (int32 Index = 0; Index < InAssignment.Participants.Num(); ++Index)
	{
		const FVeyraAssignedParticipant& Participant = InAssignment.Participants[Index];
		const FString Where = FString::Printf(TEXT("participant %d"), Index);
		if (Participant.AccountId.IsEmpty() || Accounts.Contains(Participant.AccountId))
		{
			Problems.Add(Where + TEXT(": the account is missing or appears twice"));
		}
		Accounts.Add(Participant.AccountId);
		if (Participant.DisplayName.IsEmpty())
		{
			Problems.Add(Where + TEXT(": the display name is empty"));
		}
		if (!IsLowercaseHexHash(Participant.TicketHash) || Hashes.Contains(Participant.TicketHash))
		{
			Problems.Add(Where + TEXT(": the ticket hash is not a lowercase hex SHA-256, or appears twice"));
		}
		Hashes.Add(Participant.TicketHash);
		if (Participant.Side == EVeyraTeam::A || Participant.Side == EVeyraTeam::B)
		{
			++SideCounts[Participant.Side == EVeyraTeam::A ? 0 : 1];
		}
		else
		{
			Problems.Add(Where + TEXT(": the side must be A or B"));
		}
	}
	for (const int32 Count : SideCounts)
	{
		if (Count > MaxTeamSize)
		{
			Problems.Add(FString::Printf(TEXT("a side has %d participants, more than the %d Match.json teams.maxTeamSize allows"), Count, MaxTeamSize));
		}
	}

	if (!Problems.IsEmpty())
	{
		Assignment.Reset();
		return Problems;
	}
	UE_LOG(LogVeyraMatch, Log, TEXT("Hosting match %s with %d participant(s)."), *InAssignment.MatchId, InAssignment.Participants.Num());
	Assignment = MoveTemp(InAssignment);
	return Problems;
}

void UVeyraMatchHostSubsystem::ClearAssignment()
{
	Assignment.Reset();
}
