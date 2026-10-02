// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"

/**
 * Reports and commendation on the wire (ADR-047): the player's own record for one match, and the bodies of a
 * report and a commendation. Players are named as the match recorded them, never by account.
 */
namespace VeyraBackendProtocol
{
	/** Another human participant of the match, whom a player menu opens for. */
	struct FConductPlayer
	{
		FString Name;
		/** Whether they played on the player's side, so may be commended. */
		bool bTeammate = false;
	};

	/** The answer to GET /v1/me/matches/{id}/conduct (ADR-047 §4). */
	struct FConductRecord
	{
		/** The match's other human participants, in the match's order; never a bot. */
		TArray<FConductPlayer> Players;
		/** The names the player reported in the match. */
		TArray<FString> Reported;
		/** The teammate the player commended in the match; empty when none. */
		FString Commended;
		/** The reasons a report may give, in the backend's order. */
		TArray<FString> Reasons;
		/** How many characters a report's details may hold. */
		int32 DetailsMaxCharacters = 0;
	};

	/** Reads the answer to GET /v1/me/matches/{id}/conduct. False, with the problem, if it is not one. */
	VEYRASERVICES_API bool ParseConductRecord(const FString& Body, FConductRecord& Out, FString& OutProblem);

	/** The body of POST /v1/me/matches/{id}/reports. ClientId is the client's, so a resend files nothing twice. */
	VEYRASERVICES_API FString BuildReportBody(const FString& ReportedName, const FString& Reason, const FString& Details, const FString& ClientId);

	/** The body of POST /v1/me/matches/{id}/commendation. */
	VEYRASERVICES_API FString BuildCommendationBody(const FString& Name);
}
