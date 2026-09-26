// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/StringView.h"
#include "Containers/UnrealString.h"
#include "Join/VeyraMatchAssignment.h"

/**
 * The backend's wire format as the game speaks it (ADR-005, ADR-007): credential formats, request
 * bodies and responses. Pure functions, so tests cover them without a backend. A problem this code
 * reports never quotes a response body, since bodies carry credentials.
 */
namespace VeyraBackendProtocol
{
	/** An http or https URL with a host, an optional port and no path, such as http://127.0.0.1:8080. */
	VEYRASERVICES_API bool IsBaseUrl(FStringView Text);

	/** A launch code (ADR-005 L3): "vlc_" and the base64url form of 32 bytes. */
	VEYRASERVICES_API bool IsLaunchCode(FStringView Text);

	/** A build version the backend accepts: 1 to 64 letters, digits, '.', '_', '+' or '-'. */
	VEYRASERVICES_API bool IsBuildVersion(FStringView Text);

	/** Replaces the secret part of every Veyra credential in Text, so that the text can be logged. */
	VEYRASERVICES_API FString RedactCredentials(FStringView Text);

	/** The body of POST /v1/game-sessions, which redeems a launch code. */
	VEYRASERVICES_API FString BuildRedeemBody(const FString& LaunchCode, const FString& BuildVersion);

	/** The game session a redeemed launch code gives. */
	struct FGameSession
	{
		/** The game session credential ("vgs_"). Kept in memory only. */
		FString Token;
		FString AccountId;
		FString DisplayName;
	};

	/** Reads the answer to POST /v1/game-sessions. False, with the problem, if it is not one. */
	VEYRASERVICES_API bool ParseGameSession(const FString& Body, FGameSession& Out, FString& OutProblem);

	/** The player's match, as GET /v1/me/match reports it (ADR-007 §10). */
	struct FMyMatch
	{
		/** False when the player has no active match. */
		bool bHasMatch = false;
		FString MatchId;
		/** True once the match's server is ready. Only then are Host, Port and Ticket set. */
		bool bReady = false;
		FString Host;
		int32 Port = 0;
		/** The player's join ticket ("vjt_"). Kept in memory only. */
		FString Ticket;
	};

	/** Reads the answer to GET /v1/me/match. False, with the problem, if it is not one. */
	VEYRASERVICES_API bool ParseMyMatch(const FString& Body, FMyMatch& Out, FString& OutProblem);

	/** The body of POST /v1/server/matches/{id}/result (ADR-007 §7). */
	VEYRASERVICES_API FString BuildResultBody(const FVeyraMatchResult& Result);

	/** The code in a backend error body, such as "invalid_credentials", or empty if there is none. */
	VEYRASERVICES_API FString ParseErrorCode(const FString& Body);
}
