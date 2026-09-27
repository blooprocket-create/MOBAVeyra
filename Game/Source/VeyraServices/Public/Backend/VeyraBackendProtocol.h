// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/StringView.h"
#include "Containers/UnrealString.h"
#include "Join/VeyraMatchAssignment.h"
#include "Misc/Optional.h"

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

	/** A content ID, such as "cairn": the format Game/Tuning uses for Vanguards and modes. */
	VEYRASERVICES_API bool IsContentId(FStringView Text);

	/** The player's onboarding, as GET /v1/me/profile and POST /v1/me/starter report it (ADR-010 §6). */
	struct FProfile
	{
		FString AccountId;
		FString DisplayName;
		/** False until the player has chosen a starter, which stands in for the tutorial. */
		bool bTutorialCompleted = false;
		/** Empty until chosen. */
		FString StarterVanguardId;
	};

	/** Reads a profile. False, with the problem, if it is not one. */
	VEYRASERVICES_API bool ParseProfile(const FString& Body, FProfile& Out, FString& OutProblem);

	/** Which Vanguards the player may pick, as GET /v1/me/vanguards reports it (ADR-010 §6). */
	struct FVanguardAccess
	{
		TArray<FString> Owned;
		TArray<FString> Rotation;
		/** Owned and rotation together: what champion select accepts. */
		TArray<FString> Available;
		/** What a new player chooses a starter from. */
		TArray<FString> Starters;
	};

	/** Reads the Vanguards a player may pick. False, with the problem, if it is not that. */
	VEYRASERVICES_API bool ParseVanguardAccess(const FString& Body, FVanguardAccess& Out, FString& OutProblem);

	/** Where a champion select is (ADR-010 §8). */
	enum class ESelectState : uint8
	{
		Picking,
		/** Every pick is locked, and the backend is creating the match. */
		Starting,
		Started,
		Cancelled,
	};

	/** One seat of a select, as its player sees it. */
	struct FSelectSeat
	{
		FString DisplayName;
		/** "A" or "B". */
		FString Side;
		/** The seat of the player reading the select. */
		bool bYou = false;
		/** The hovered Vanguard; empty for none, and always empty for the enemy team's seats. */
		FString Hover;
		/** The locked Vanguard; empty until locked. */
		FString Locked;
	};

	/** A champion select, as one of its players sees it. */
	struct FSelect
	{
		FString Id;
		/** "practice"; M6b adds the queued kinds. */
		FString Kind;
		FString Mode;
		ESelectState State = ESelectState::Picking;
		/** What was left of the pick timer when the backend answered, by the backend's clock. */
		double RemainingSeconds = 0.0;
		TArray<FSelectSeat> Seats;
		/** Set once started. */
		FString MatchId;
		/** Set once cancelled, such as "timed_out". */
		FString CancelReason;

		/** The reading player's seat; null if they have none. */
		VEYRASERVICES_API const FSelectSeat* FindYou() const;
	};

	/**
	 * Reads {"select": ...}, the answer of every select route. OutSelect is unset when the select is
	 * null: the player is in no select. False, with the problem, if the answer is not a select.
	 */
	VEYRASERVICES_API bool ParseSelect(const FString& Body, TOptional<FSelect>& OutSelect, FString& OutProblem);

	/** How the player's match went, as GET /v1/me/matches/{id} reports it (ADR-010 §3). */
	struct FMatchOutcome
	{
		FString MatchId;
		FString Mode;
		/** "standard" or "practice". */
		FString Rules;
		/** "allocating", "ready", "ended" or "failed". */
		FString State;
		FString Side;
		FString VanguardId;
		/** Why the match failed without a result, such as "server_exited"; empty otherwise. */
		FString FailureReason;
		/** True once the match server's result is recorded. The fields below are set only then. */
		bool bHasResult = false;
		/** Such as "host_ended". */
		FString EndReason;
		/** "A", "B", or empty when no side won. */
		FString Winner;
		double DurationSeconds = 0.0;
		bool bJoined = false;
		bool bConnectedAtEnd = false;

		/** Whether the match still holds its players: allocating or ready. */
		VEYRASERVICES_API bool IsActive() const;
	};

	/** Reads the answer to GET /v1/me/matches/{id}. False, with the problem, if it is not one. */
	VEYRASERVICES_API bool ParseMatchOutcome(const FString& Body, FMatchOutcome& Out, FString& OutProblem);

	/** The body of POST /v1/me/starter, PUT /v1/me/select/hover and POST /v1/me/select/lock. */
	VEYRASERVICES_API FString BuildVanguardBody(const FString& VanguardId);

	/** The body of POST /v1/server/matches/{id}/result (ADR-007 §7). */
	VEYRASERVICES_API FString BuildResultBody(const FVeyraMatchResult& Result);

	/** The code in a backend error body, such as "invalid_credentials", or empty if there is none. */
	VEYRASERVICES_API FString ParseErrorCode(const FString& Body);
}
