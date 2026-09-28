// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Join/VeyraMatchHostSubsystem.h"

#include "Engine/Engine.h"
#include "Rules/VeyraMatchRules.h"
#include "Tuning/VeyraMatchTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
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
	if (!InAssignment.Mode.IsValid())
	{
		Problems.Add(TEXT("the assignment has no mode"));
	}
	// A practice match has a host who plays in it; a standard match has none (ADR-010 §7).
	const bool bPractice = InAssignment.Rules == EVeyraMatchRules::Practice;
	if (bPractice && !InAssignment.Participants.ContainsByPredicate([&InAssignment](const FVeyraAssignedParticipant& Participant) {
			return Participant.AccountId == InAssignment.HostAccountId;
		}))
	{
		Problems.Add(TEXT("a practice match's host must be on its roster"));
	}
	if (!bPractice && !InAssignment.HostAccountId.IsEmpty())
	{
		Problems.Add(TEXT("only a practice match has a host"));
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
		const FString VanguardProblem = VeyraMatchRules::CheckAssignedVanguard(Participant.VanguardId,
			Participant.VanguardId.IsValid() ? UVeyraVanguardsTuningSubsystem::FindVanguard(Participant.VanguardId) : nullptr, UE_BUILD_SHIPPING != 0);
		if (!VanguardProblem.IsEmpty())
		{
			Problems.Add(Where + TEXT(": ") + VanguardProblem);
		}
	}
	// Bots are practice targets for now (ADR-010 §7); they take places on their sides like anyone.
	if (!bPractice && !InAssignment.Bots.IsEmpty())
	{
		Problems.Add(TEXT("only a practice match has bots"));
	}
	for (int32 Index = 0; Index < InAssignment.Bots.Num(); ++Index)
	{
		const FVeyraAssignedBot& Bot = InAssignment.Bots[Index];
		const FString Where = FString::Printf(TEXT("bot %d"), Index);
		if (Bot.Side == EVeyraTeam::A || Bot.Side == EVeyraTeam::B)
		{
			++SideCounts[Bot.Side == EVeyraTeam::A ? 0 : 1];
		}
		else
		{
			Problems.Add(Where + TEXT(": the side must be A or B"));
		}
		const FString VanguardProblem = VeyraMatchRules::CheckAssignedVanguard(Bot.VanguardId,
			Bot.VanguardId.IsValid() ? UVeyraVanguardsTuningSubsystem::FindVanguard(Bot.VanguardId) : nullptr, UE_BUILD_SHIPPING != 0);
		if (!VanguardProblem.IsEmpty())
		{
			Problems.Add(Where + TEXT(": ") + VanguardProblem);
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
	UE_LOG(LogVeyraMatch, Log, TEXT("Hosting %s match %s (%s) with %d participant(s) and %d bot(s)."), bPractice ? TEXT("practice") : TEXT("standard"),
		*InAssignment.MatchId, *InAssignment.Mode.ToString(), InAssignment.Participants.Num(), InAssignment.Bots.Num());
	Assignment = MoveTemp(InAssignment);
	return Problems;
}

void UVeyraMatchHostSubsystem::ClearAssignment()
{
	Assignment.Reset();
}
