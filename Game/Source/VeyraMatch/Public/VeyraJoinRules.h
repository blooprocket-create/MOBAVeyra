// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Hash/Blake3.h"

class FVeyraMatchRoster;

/**
 * What a client must bring to join a match. A client reports the composite hash of its tuning in
 * its login options, and the server refuses any build whose tuning differs from its own
 * (ADR-006 §6). A server hosting an assigned match also needs the client's join ticket (ADR-007).
 */
namespace VeyraJoinRules
{
	/** The login option that carries the client's composite tuning hash. */
	inline constexpr const TCHAR* TuningHashOption = TEXT("VeyraTuning");

	/** The login option that carries the client's join ticket (ADR-007 §4). */
	inline constexpr const TCHAR* TicketOption = TEXT("VeyraTicket");

	/** The login option, development builds only, that asks for a Vanguard (ADR-008 §8); Shipping servers ignore it. */
	inline constexpr const TCHAR* VanguardOption = TEXT("VeyraVanguard");

	/** The login option for a join ticket, as "VeyraTicket=<ticket>". */
	VEYRAMATCH_API FString MakeTicketOption(const FString& Ticket);

	/**
	 * Why a client with these login options may not join the match Roster describes, or an empty
	 * string when it may, with OutAccountId set to its rostered account. The ticket must hash to a
	 * rostered participant who is not connected now and has not joined before: rejoining waits for
	 * reconnect (Match Flow Bible §4). The message never contains the ticket.
	 */
	VEYRAMATCH_API FString CheckTicket(const FString& Options, const FVeyraMatchRoster& Roster, FString& OutAccountId);

	/** The login option for a composite tuning hash, as "VeyraTuning=<hex>". */
	VEYRAMATCH_API FString MakeTuningHashOption(const FBlake3Hash& CompositeHash);

	/**
	 * Why a client with these login options may not join a server whose composite tuning hash is
	 * ServerHash, or an empty string when it may.
	 */
	VEYRAMATCH_API FString CheckTuningHash(const FString& Options, const FBlake3Hash& ServerHash);

	/**
	 * Why a server refuses every login, or an empty string. A server hosting an assigned match
	 * admits clients by ticket. Without an assignment, development builds accept direct connections
	 * (ADR-005 step 2) and Shipping builds refuse them (ADR-007 §9).
	 */
	VEYRAMATCH_API FString CheckDirectConnect(bool bServerHasAssignment);
}
