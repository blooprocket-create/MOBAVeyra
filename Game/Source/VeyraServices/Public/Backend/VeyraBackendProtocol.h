// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/StringView.h"
#include "Containers/UnrealString.h"
#include "Misc/DateTime.h"
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
		/** Every released Vanguard, owned or not: what a draft's bans may name (ADR-041 §1). */
		TArray<FString> Released;
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

	/**
	 * Where a picking select is (ADR-041 §1–§2): a draft's ban turn, picking (a draft's pick turn, or
	 * any other select's whole time), or the final window after the last lock, in which locked
	 * teammates may still trade before the match starts.
	 */
	enum class ESelectPhase : uint8
	{
		Banning,
		Picking,
		Final,
	};

	/** A draft's turn: Count bans or picks by Side, Done of them made (ADR-041 §1). */
	struct FSelectTurn
	{
		bool bBan = false;
		/** "A" or "B". */
		FString Side;
		int32 Count = 0;
		int32 Done = 0;
	};

	/** A Vanguard a draft's side banned: neither team may pick it. */
	struct FSelectBan
	{
		/** "A" or "B". */
		FString Side;
		FString VanguardId;
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
		/**
		 * The seat's starting Flux Spells in slot order, an empty string for an empty slot (ADR-015 §5);
		 * only the reading player's own seat carries them.
		 */
		TArray<FString> FluxSpells;
		/** The ban the seat is considering in its draft's ban turn; always empty for the enemy team's seats. */
		FString BanHover;
		/** Whether the seat bans or picks in the draft's current turn. */
		bool bActing = false;
		/** Whether the seat offers the reading player a trade of locked Vanguards (ADR-041 §2). */
		bool bOffersYou = false;
		/** Whether the reading player offers the seat one. */
		bool bOfferedByYou = false;
	};

	/** A bot a custom lobby put in its select: a seat locked from the start (ADR-021 §3). */
	struct FSelectBot
	{
		/** "A" or "B". */
		FString Side;
		FString VanguardId;
		/** "beginner" or "intermediate". */
		FString Difficulty;
	};

	/** A champion select, as one of its players sees it. */
	struct FSelect
	{
		FString Id;
		/** "practice", "casual" (matchmade), "draft" (matchmade Draft Pick) or "custom" (a custom lobby's). */
		FString Kind;
		FString Mode;
		ESelectState State = ESelectState::Picking;
		ESelectPhase Phase = ESelectPhase::Picking;
		/** A draft's current turn; unset outside one. */
		TOptional<FSelectTurn> Turn;
		/** A draft's bans, in order, which both teams see. */
		TArray<FSelectBan> Bans;
		/** What was left of the current phase or turn when the backend answered, by the backend's clock. */
		double RemainingSeconds = 0.0;
		/** The current phase or turn's full length, for a countdown bar. */
		double PickSeconds = 0.0;
		TArray<FSelectSeat> Seats;
		/** A custom select's bots, in each side's seat order; empty for the other kinds. */
		TArray<FSelectBot> Bots;
		/** Set once started. */
		FString MatchId;
		/** Set once cancelled, such as "timed_out". */
		FString CancelReason;

		/** The reading player's seat; null if they have none. */
		VEYRASERVICES_API const FSelectSeat* FindYou() const;

		/** Whether the reading player bans now: the draft's ban turn names them. */
		VEYRASERVICES_API bool YouBan() const;

		/** Whether the reading player may lock a pick now: they have not, and a draft's pick turn names them. */
		VEYRASERVICES_API bool YouMayLock() const;

		/** Whether a side banned the Vanguard. */
		VEYRASERVICES_API bool IsBanned(const FString& VanguardId) const;
	};

	/**
	 * Reads {"select": ...}, the answer of every select route. OutSelect is unset when the select is
	 * null: the player is in no select. False, with the problem, if the answer is not a select.
	 */
	VEYRASERVICES_API bool ParseSelect(const FString& Body, TOptional<FSelect>& OutSelect, FString& OutProblem);

	/** One player's line on a verified result's scoreboard (ADR-017 §5), with no account. */
	struct FPlayerOutcome
	{
		/** "A" or "B". */
		FString Side;
		FString Name;
		FString VanguardId;
		/** Whether this is the player who asked. */
		bool bYou = false;
		/** The recorded statistics, with the final items and Flux Spells. */
		FVeyraPlayerStatistics Statistics;
	};

	/** A Flux Well secured, on a verified result (Match Statistics Bible §5). */
	struct FWellOutcome
	{
		int32 Site = 0;
		/** "A" or "B". */
		FString Side;
		/** On the match clock. */
		double AtSeconds = 0.0;
	};

	/** How the player's match went, as GET /v1/me/matches/{id} reports it (ADR-010 §3). */
	struct FMatchOutcome
	{
		FString MatchId;
		FString Mode;
		/** "standard", "practice" or "custom". */
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
		/** A loss the player's own absence earned, whatever its team's result (Match Flow Bible §6; UX-51). */
		bool bPersonalLoss = false;
		/** Whether the result carries a scoreboard: a result from an older server, or one nobody played, has none. */
		bool bHasScoreboard = false;
		/** The scoreboard's lines, as the server reported them: side A first, in seat order. */
		TArray<FPlayerOutcome> Players;
		/** Every Flux Well secured, in order; empty when none was, or the server sent none. */
		TArray<FWellOutcome> Wells;

		/** Whether the match still holds its players: allocating or ready. */
		VEYRASERVICES_API bool IsActive() const;
	};

	/** Reads the answer to GET /v1/me/matches/{id}. False, with the problem, if it is not one. */
	VEYRASERVICES_API bool ParseMatchOutcome(const FString& Body, FMatchOutcome& Out, FString& OutProblem);

	/**
	 * What Match History lists (Pre-Game Client UX Bible 64): each empty field matches every match. Outcome
	 * is "win", "loss" or "no_contest".
	 */
	struct FHistoryFilter
	{
		FString VanguardId;
		FString Mode;
		FString Outcome;

		bool operator==(const FHistoryFilter&) const = default;
	};

	/** One completed match as Match History lists it (UX-51). */
	struct FHistoryEntry
	{
		FString MatchId;
		FString Mode;
		FString Rules;
		FDateTime EndedAt;
		double DurationSeconds = 0.0;
		FString Side;
		/** Empty for a match from before matches carried Vanguards. */
		FString VanguardId;
		/** "win", "loss" or "no_contest": the player's own. */
		FString Outcome;
		/** Whether Outcome is the player's own loss for absence rather than its team's (UX-51). */
		bool bPersonalLoss = false;
	};

	/** A page of Match History, newest first, and the cursor of the next; Next is empty on the last page. */
	struct FHistoryPage
	{
		TArray<FHistoryEntry> Entries;
		FString Next;
		/** Every mode the player has a completed match in, whatever the filter: the mode filter's choices (UX-67). */
		TArray<FString> Modes;
	};

	/** GET /v1/me/matches with Filter, from Cursor (empty for the first page). */
	VEYRASERVICES_API FString HistoryPath(const FHistoryFilter& Filter, const FString& Cursor);

	/** Reads the answer to GET /v1/me/matches. False, with the problem, if it is not one. */
	VEYRASERVICES_API bool ParseHistoryPage(const FString& Body, FHistoryPage& Out, FString& OutProblem);

	/** A mode the Play screen offers, as GET /v1/modes reports it (ADR-010 §10). */
	/** The Play page's group for a mode (ADR-039 §6); Customs are the client's own entries, no queue's. */
	enum class EModeCategory : uint8
	{
		Ranked,
		Casual,
		AI,
	};

	struct FModeInfo
	{
		FString Id;
		bool bEnabled = false;
		int32 HumanPlayersPerTeam = 0;
		/** Whether it has a matchmaker; a mode without one is shown as not yet available. */
		bool bMatchmade = false;
		/** Whether its matches put its humans against an enemy AI team (ADR-039 §2). */
		bool bVersusAI = false;
		EModeCategory Category = EModeCategory::Casual;
	};

	/** Reads the answer to GET /v1/modes. False, with the problem, if it is not one. */
	VEYRASERVICES_API bool ParseModes(const FString& Body, TArray<FModeInfo>& Out, FString& OutProblem);

	/** Where a party is in matchmaking (Parties & Social Bible §2–3). Every status but Idle locks it. */
	enum class EPartyStatus : uint8
	{
		Idle,
		Queued,
		/** In Match Found: every player must accept. */
		Found,
		Selecting,
	};

	struct FPartyMember
	{
		FString AccountId;
		FString DisplayName;
		bool bReady = false;
		bool bLeader = false;
	};

	/** Who may join a party without an invitation (Parties & Social Bible §1). The leader's to choose. */
	enum class EPartyPrivacy : uint8
	{
		/** Joining needs an invitation from a member. */
		Private,
		/** A member's friends may join directly while it has room. */
		Public,
	};

	/** The player's party, as the party routes report it. */
	struct FParty
	{
		FString Id;
		/** Empty until the leader chooses one. */
		FString Mode;
		/** Private unless the leader made it Public. An answer without it reads as Private. */
		EPartyPrivacy Privacy = EPartyPrivacy::Private;
		EPartyStatus Status = EPartyStatus::Idle;
		/** How long it has been in matchmaking when the backend answered, by the backend's clock. */
		double QueuedSeconds = 0.0;
		TArray<FPartyMember> Members;

		VEYRASERVICES_API const FPartyMember* Find(const FString& AccountId) const;
		/** Whether every member is Ready. */
		VEYRASERVICES_API bool AllReady() const;
	};

	/**
	 * Reads {"party": ...}, the answer of every party route. OutParty is unset when the party is null:
	 * the player has none. False, with the problem, if the answer is not a party.
	 */
	VEYRASERVICES_API bool ParseParty(const FString& Body, TOptional<FParty>& OutParty, FString& OutProblem);

	/** A match found, as one of its players sees it (Parties & Social Bible §3). */
	struct FMatchFound
	{
		FString Id;
		FString Mode;
		/** "pending", "accepted" or "abandoned". */
		FString State;
		/** What was left of the acceptance timer when the backend answered, by the backend's clock. */
		double RemainingSeconds = 0.0;
		int32 Accepted = 0;
		int32 Total = 0;
		/** The player's own answer: "pending", "accepted" or "declined". */
		FString You;
		/** Set once accepted: the champion select it opened. */
		FString SelectId;
		/** Set once abandoned, such as "declined". */
		FString AbandonReason;
	};

	/** Reads {"matchFound": ...}. OutFound is unset when it is null. False, with the problem, if it is not one. */
	VEYRASERVICES_API bool ParseMatchFound(const FString& Body, TOptional<FMatchFound>& OutFound, FString& OutProblem);

	/** An account as the social routes name it. */
	struct FAccount
	{
		FString Id;
		FString DisplayName;

		bool operator==(const FAccount&) const = default;
	};

	/** GET /v1/accounts for the account whose display name is exactly DisplayName. */
	VEYRASERVICES_API FString AccountLookupPath(const FString& DisplayName);

	/** Reads the answer to GET /v1/accounts. False, with the problem, if it is not an account. */
	VEYRASERVICES_API bool ParseAccount(const FString& Body, FAccount& Out, FString& OutProblem);

	/** A friend whose Public party the player may join without an invitation (ADR-043 §3). */
	struct FJoinableParty
	{
		FString AccountId;
		FString PartyId;

		bool operator==(const FJoinableParty&) const = default;
	};

	/** The player's friends and friend requests, as GET /v1/friends reports them (Parties & Social Bible §1), each by name. */
	struct FFriends
	{
		TArray<FAccount> Friends;
		/** Requests to the player, which they may accept or decline. */
		TArray<FAccount> Incoming;
		/** The player's own requests, not answered yet. */
		TArray<FAccount> Outgoing;
		/** The friends whose party the player may join directly, sorted by account. Empty from a backend that does not report them. */
		TArray<FJoinableParty> JoinableParties;

		/** The party AccountId's line offers to join, or null. */
		VEYRASERVICES_API const FString* JoinablePartyOf(const FString& AccountId) const;

		bool operator==(const FFriends&) const = default;
	};

	VEYRASERVICES_API bool ParseFriends(const FString& Body, FFriends& Out, FString& OutProblem);

	/**
	 * Reads the answer to POST /v1/friends/requests: "requested", or "friends" when the other player had
	 * already asked, which makes them friends at once.
	 */
	VEYRASERVICES_API bool ParseFriendRequestOutcome(const FString& Body, FString& OutOutcome, FString& OutProblem);

	/** The body of POST /v1/friends/requests, POST /v1/lobby/invites, POST /v1/party/invites and PUT /v1/party/leader. */
	VEYRASERVICES_API FString BuildAccountBody(const FString& AccountId);

	/** What sits in a custom lobby's seat. */
	enum class ELobbySeatKind : uint8
	{
		Empty,
		Human,
		Bot,
	};

	/** One seat of a custom lobby (ADR-021 §2): empty, a human, or a bot. */
	struct FLobbySeat
	{
		/** "A" or "B". */
		FString Side;
		/** From 0, on its side. */
		int32 Index = 0;
		ELobbySeatKind Kind = ELobbySeatKind::Empty;
		/** A human's; empty otherwise. */
		FString AccountId;
		FString DisplayName;
		bool bHost = false;
		/** A bot's; empty otherwise. */
		FString VanguardId;
		/** A bot's: "beginner" or "intermediate". */
		FString Difficulty;
	};

	/** The player's custom lobby, as the lobby routes report it (ADR-021; Custom Matches Bible §1–§4). */
	struct FLobby
	{
		FString Id;
		FString HostAccountId;
		/** In the champion select its launch opened, and fixed until that ends. */
		bool bSelecting = false;
		int32 PlayersPerSide = 0;
		/** Whether destroying a Prime Well wins (§4); off while a side has nobody. */
		bool bVictoryEnabled = false;
		/** The starting Gold the host set; unset plays the game's own. */
		TOptional<double> StartingGold;
		/** The range starting Gold may be set in. */
		double StartingGoldMin = 0.0;
		double StartingGoldMax = 0.0;
		/** Side A's seats, then side B's, each in order: PlayersPerSide on each side. */
		TArray<FLobbySeat> Seats;
		/** What a bot may be: the released Vanguards, sorted, and the difficulties, easiest first. */
		TArray<FString> BotVanguards;
		TArray<FString> BotDifficulties;

		/** AccountId's seat; null if they are not in the lobby. */
		VEYRASERVICES_API const FLobbySeat* FindMember(const FString& AccountId) const;
	};

	/**
	 * Reads {"lobby": ...}, the answer of the lobby routes. OutLobby is unset when the lobby is null: the
	 * player is in none. False, with the problem, if the answer is not a lobby.
	 */
	VEYRASERVICES_API bool ParseLobby(const FString& Body, TOptional<FLobby>& OutLobby, FString& OutProblem);

	/** An invitation into another player's custom lobby. */
	struct FLobbyInvite
	{
		FString Id;
		FString LobbyId;
		FAccount Inviter;

		bool operator==(const FLobbyInvite&) const = default;
	};

	/** Reads the answer to GET /v1/lobby/invites. False, with the problem, if it is not that. */
	VEYRASERVICES_API bool ParseLobbyInvites(const FString& Body, TArray<FLobbyInvite>& Out, FString& OutProblem);

	/** An invitation into another player's party (Parties & Social Bible §1). */
	struct FPartyInvite
	{
		FString Id;
		FString PartyId;
		FAccount Inviter;

		bool operator==(const FPartyInvite&) const = default;
	};

	/** Reads the answer to GET /v1/party/invites. False, with the problem, if it is not that. */
	VEYRASERVICES_API bool ParsePartyInvites(const FString& Body, TArray<FPartyInvite>& Out, FString& OutProblem);

	/** Reads the answer to GET /v1/blocks: the players the player blocked (§6). False, with the problem, if it is not that. */
	VEYRASERVICES_API bool ParseBlocks(const FString& Body, TArray<FAccount>& Out, FString& OutProblem);

	/** PUT and DELETE /v1/lobby/seats/{side}/{index}/bot. */
	VEYRASERVICES_API FString LobbyBotPath(const FString& Side, int32 Index);

	/** The body of PUT /v1/lobby/members/{accountId}/seat. */
	VEYRASERVICES_API FString BuildSeatBody(const FString& Side, int32 Index);

	/** The body of PUT /v1/lobby/seats/{side}/{index}/bot. */
	VEYRASERVICES_API FString BuildBotBody(const FString& VanguardId, const FString& Difficulty);

	/** The body of PUT /v1/lobby/settings; an unset StartingGold plays the game's own. */
	VEYRASERVICES_API FString BuildLobbySettingsBody(bool bVictoryEnabled, TOptional<double> StartingGold);

	/** The body of PUT /v1/party/mode. */
	VEYRASERVICES_API FString BuildModeBody(const FString& ModeId);

	/** The body of PUT /v1/party/ready. */
	VEYRASERVICES_API FString BuildReadyBody(bool bReady);

	/** The body of PUT /v1/party/privacy. */
	VEYRASERVICES_API FString BuildPrivacyBody(EPartyPrivacy Privacy);

	/** The body of POST /v1/me/starter, the select's hover and lock, and its ban hover and ban. */
	VEYRASERVICES_API FString BuildVanguardBody(const FString& VanguardId);

	/** The body of POST /v1/me/select/trade and its accept and decline: the teammate's seat, by its place in the select's seats. */
	VEYRASERVICES_API FString BuildTradeBody(int32 Seat);

	/** The body of PUT /v1/me/select/spells: the starting Flux Spells in slot order, "" for an empty slot. */
	VEYRASERVICES_API FString BuildFluxSpellsBody(TConstArrayView<FString> Spells);

	/** The body of POST /v1/server/matches/{id}/result (ADR-007 §7). */
	VEYRASERVICES_API FString BuildResultBody(const FVeyraMatchResult& Result);

	/** The code in a backend error body, such as "invalid_credentials", or empty if there is none. */
	VEYRASERVICES_API FString ParseErrorCode(const FString& Body);
}
