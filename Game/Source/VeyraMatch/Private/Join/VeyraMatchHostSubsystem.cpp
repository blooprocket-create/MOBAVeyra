// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Join/VeyraMatchHostSubsystem.h"

#include "Engine/Engine.h"
#include "Rules/VeyraMatchRules.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
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
	// A practice or custom match has a host who plays in it; a standard match has none (ADR-010 §7; ADR-021 §3).
	const bool bHosted = VeyraMatchRules::HasHost(InAssignment.Rules);
	if (bHosted && !InAssignment.Participants.ContainsByPredicate([&InAssignment](const FVeyraAssignedParticipant& Participant) {
			return Participant.AccountId == InAssignment.HostAccountId;
		}))
	{
		Problems.Add(TEXT("a practice or custom match's host must be on its roster"));
	}
	if (!bHosted && !InAssignment.HostAccountId.IsEmpty())
	{
		Problems.Add(TEXT("only a practice or custom match has a host"));
	}
	// A custom match, and only one, carries its session's rules; its starting Gold is Gold (ADR-021 §3).
	const bool bCustom = InAssignment.Rules == EVeyraMatchRules::Custom;
	if (bCustom != InAssignment.Custom.IsSet())
	{
		Problems.Add(TEXT("a custom match, and only a custom match, has session settings"));
	}
	if (InAssignment.Custom.IsSet() && InAssignment.Custom->StartingGold.IsSet()
		&& (!FMath::IsFinite(*InAssignment.Custom->StartingGold) || *InAssignment.Custom->StartingGold < 0.0))
	{
		Problems.Add(TEXT("the session's starting Gold is not a finite amount of at least 0"));
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
		const FString SpellsProblem = VeyraMatchRules::CheckAssignedFluxSpells(Participant.FluxSpells, UVeyraAbilitiesTuningSubsystem::Get().FluxSpells.Roster);
		if (!SpellsProblem.IsEmpty())
		{
			Problems.Add(Where + TEXT(": ") + SpellsProblem);
		}
	}
	// Bots play in hosted matches (ADR-010 §7; ADR-021 §2), and as a co-op match's enemy team, on a side no human
	// plays (ADR-038 §4); they take places on their sides like anyone.
	const bool bEnemyTeam = !InAssignment.Bots.ContainsByPredicate([&InAssignment](const FVeyraAssignedBot& Bot) {
		return InAssignment.Participants.ContainsByPredicate([&Bot](const FVeyraAssignedParticipant& Participant) { return Participant.Side == Bot.Side; });
	});
	if (!bHosted && !InAssignment.Bots.IsEmpty() && !bEnemyTeam)
	{
		Problems.Add(TEXT("only a practice or custom match has bots beside humans; a co-op match's sit on a side no human plays"));
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
	UE_LOG(LogVeyraMatch, Log, TEXT("Hosting %s match %s (%s) with %d participant(s) and %d bot(s)."),
		*StaticEnum<EVeyraMatchRules>()->GetNameStringByValue(static_cast<int64>(InAssignment.Rules)),
		*InAssignment.MatchId, *InAssignment.Mode.ToString(), InAssignment.Participants.Num(), InAssignment.Bots.Num());
	Assignment = MoveTemp(InAssignment);
	return Problems;
}

void UVeyraMatchHostSubsystem::ClearAssignment()
{
	Assignment.Reset();
}
