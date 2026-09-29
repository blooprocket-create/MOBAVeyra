// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraJoinRules.h"

#include "Hash/VeyraSha256.h"
#include "Join/VeyraMatchRoster.h"
#include "Kismet/GameplayStatics.h"

namespace VeyraJoinRules
{
FString MakeTuningHashOption(const FBlake3Hash& CompositeHash)
{
	return FString::Printf(TEXT("%s=%s"), TuningHashOption, *LexToString(CompositeHash));
}

FString CheckTuningHash(const FString& Options, const FBlake3Hash& ServerHash)
{
	const FString Reported = UGameplayStatics::ParseOption(Options, TuningHashOption);
	const FString Expected = LexToString(ServerHash);
	if (Reported.IsEmpty())
	{
		return FString::Printf(TEXT("The client did not report its tuning hash; this server runs %s."), *Expected);
	}
	if (!Reported.Equals(Expected, ESearchCase::IgnoreCase))
	{
		return FString::Printf(TEXT("Tuning differs between builds: the client has %s and this server has %s."), *Reported, *Expected);
	}
	return FString();
}

FString MakeTicketOption(const FString& Ticket)
{
	return FString::Printf(TEXT("%s=%s"), TicketOption, *Ticket);
}

FString CheckTicket(const FString& Options, const FVeyraMatchRoster& Roster, FString& OutAccountId)
{
	OutAccountId.Reset();
	const FString Ticket = UGameplayStatics::ParseOption(Options, TicketOption);
	if (Ticket.IsEmpty())
	{
		return TEXT("This match admits players by join ticket, and the client sent none.");
	}
	const FVeyraAssignedParticipant* Participant = Roster.FindByTicketHash(VeyraHash::Sha256Hex(Ticket));
	if (!Participant)
	{
		return TEXT("The join ticket is not valid for this match.");
	}
	if (Roster.IsConnected(Participant->AccountId))
	{
		return FString::Printf(TEXT("%s is already connected."), *Participant->DisplayName);
	}
	// One who left may come back, to the Vanguard it left (Match Flow Bible §3; ADR-019 §1).
	OutAccountId = Participant->AccountId;
	return FString();
}

FString CheckDirectConnect(bool bServerHasAssignment)
{
	if constexpr (UE_BUILD_SHIPPING)
	{
		if (!bServerHasAssignment)
		{
			return TEXT("This server hosts no assigned match, and Shipping builds accept no direct connections.");
		}
	}
	return FString();
}
}
