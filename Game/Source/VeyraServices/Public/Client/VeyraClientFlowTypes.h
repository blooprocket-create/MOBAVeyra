// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Backend/VeyraBackendProtocol.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Handoff/VeyraLaunchHandshake.h"
#include "Misc/Optional.h"

/**
 * Where a signed-in game is, from the launch to the shell and through a match (ADR-004, ADR-010 §2).
 * Each state has one screen; the in-match state has the match's own presentation.
 */
enum class EVeyraClientState : uint8
{
	/** Waiting for the launch code on standard input, then redeeming it. */
	SigningIn,
	/** Signing in failed. A launch code works once, so only Quit helps; the launcher offers Retry. */
	SignInFailed,
	/** Signed in: finding out whether the player has a match, a select, or a starter to choose. */
	Loading,
	/** The one-time starter choice that stands in for the tutorial (ADR-010 §6). */
	StarterChoice,
	/** Home, Play and the mode choice, the party, and its queue. */
	Shell,
	/** A custom lobby (ADR-021): its seats, bots and rules, until its host starts champion select. */
	Lobby,
	/** Matchmaking found a match, and every player must accept it (Parties & Social Bible §3). */
	MatchFound,
	/** Champion select. */
	Selecting,
	/** The match exists and its server is starting. */
	MatchStarting,
	/** Travelling to the match server. */
	Connecting,
	InMatch,
	/** Travelling back to the front end after a match, or after losing it. */
	Returning,
	/** Waiting for the backend's verified result (UX-15). */
	AwaitingResults,
	Results,
	/** The player's match runs without them: Reconnect is all they may do (UX-17). */
	ReconnectOnly,
	/** The backend no longer accepts the game session; only Quit helps. */
	SessionEnded,
};

/** What the player, or a script standing in for them, can ask for. */
enum class EVeyraClientIntent : uint8
{
	ChooseStarter,
	StartPractice,
	/** Chooses the party's mode, making a party of one if the player has none. The leader's. */
	SelectMode,
	/** Readies up, or stops being Ready, for the party's mode. */
	SetReady,
	/** Queues the party. The leader's, once every member is Ready. */
	FindMatch,
	/** Takes the party out of the queue. The leader's. */
	CancelQueue,
	AcceptMatch,
	DeclineMatch,
	HoverVanguard,
	LockVanguard,
	/** Leaves a matchmade champion select, which cancels it for everyone (a dodge). */
	LeaveSelect,
	/** Chooses a starting Flux Spell for one slot, before or after lock-in (ADR-015 §5). */
	ChooseFluxSpell,
	/** Considers a ban in the player's draft ban turn, which their team sees (ADR-041 §1). */
	HoverBan,
	/** Bans a Vanguard for the player's side in their draft ban turn. */
	BanVanguard,
	/** Offers a locked teammate the player's locked Vanguard for theirs (ADR-041 §2). */
	OfferTrade,
	/** Accepts or declines a teammate's trade offer. */
	AnswerTrade,
	Reconnect,
	ContinueFromResults,
	/** Repeats the step whose problem is showing. */
	Retry,
	/** Always allowed: the game never quits by itself. */
	Quit,
	/** Reads Match History's first page with a filter (UX-51, UX-64). */
	LoadHistory,
	/** Reads Match History's next page (UX-67). */
	LoadMoreHistory,
	/** Opens a listed match into its Scoreboard and Detailed Statistics. */
	OpenHistoryMatch,
	/** Goes back from an opened match to the list. */
	CloseHistoryMatch,
	/** Opens a custom lobby the player hosts (ADR-021). */
	CreateLobby,
	/** Joins the lobby an invitation is from. */
	AcceptLobbyInvite,
	DeclineLobbyInvite,
	/** The host's: invites a friend into the lobby. */
	InviteToLobby,
	LeaveLobby,
	/** The host's: removes another human from the lobby. */
	KickFromLobby,
	/** The host's: puts a human in an empty seat (Custom Matches Bible §1). */
	MoveInLobby,
	/** The host's: puts a bot in a seat, or changes the bot there (§2–§3). */
	SetLobbyBot,
	RemoveLobbyBot,
	/** The host's: victory and starting Gold (§4). */
	SetLobbySettings,
	/** The host's: starts champion select for everyone in the lobby. */
	LaunchLobby,
	/** Asks a player, by display name, to be friends (Parties & Social Bible §1). */
	SendFriendRequest,
	/** Accepts or declines a friend request to the player. */
	AnswerFriendRequest,
	RemoveFriend,
	/** Keeps this device's settings or the account's, when both changed (ADR-024 §1). Whenever the choice shows. */
	ResolveSettingsConflict,
};

/** Which kind of world the client just loaded. */
enum class EVeyraClientWorld : uint8
{
	/** The front end, or any world not connected to a server. */
	FrontEnd,
	/** A world connected to a match server. */
	Match,
};

VEYRASERVICES_API const TCHAR* LexToString(EVeyraClientState State);
VEYRASERVICES_API const TCHAR* LexToString(EVeyraClientIntent Intent);

/** Something that went wrong, shown on the current screen. */
struct FVeyraClientProblem
{
	/**
	 * Stable, for presentation and tests: the backend's error code, such as "not_available", or the
	 * client's own: "backend_unreachable", "bad_answer", "session_ended", "match_not_ready",
	 * "travel_failed", or a launch-handshake failure code.
	 */
	FString Code;
	/** What happened, for the log and the screen. It never quotes a response body or a credential. */
	FString Message;
	/** Whether Retry repeats the failed step. */
	bool bCanRetry = false;
};

/** Match History as the player has read it (Pre-Game Client UX Bible 51, 64, 67). */
struct FVeyraMatchHistory
{
	VeyraBackendProtocol::FHistoryFilter Filter;
	/** The pages read so far for Filter, newest first. */
	TArray<VeyraBackendProtocol::FHistoryEntry> Entries;
	/** The next page's cursor; empty on the last page. */
	FString Next;
	/** Every mode the player has a completed match in, from the backend: the mode filter's choices. */
	TArray<FString> Modes;
	/** Whether the first page for Filter has been read. */
	bool bLoaded = false;
	/** A match opened from the list; unset while none is. */
	TOptional<VeyraBackendProtocol::FMatchOutcome> Opened;
};

/**
 * The player's friends, friend requests and lobby invitations as last read (Parties & Social Bible §1),
 * in the shell and the lobby. Reading them never stops the flow: a failed read is tried again later.
 */
struct FVeyraSocial
{
	/** Whether they have been read at all. */
	bool bLoaded = false;
	VeyraBackendProtocol::FFriends Friends;
	TArray<VeyraBackendProtocol::FLobbyInvite> LobbyInvites;
	/**
	 * What came of the player's last friend request, for the friends panel rather than the screen's
	 * problem: "friend_requested", "friend_added", or the backend's refusal, such as "account_not_found"
	 * or "already_friends". Empty for none.
	 */
	FString Feedback;
	/** The name that request was for. */
	FString FeedbackName;
};

/** Everything the presentation shows about the flow. Only the flow changes it. */
struct FVeyraClientSnapshot
{
	EVeyraClientState State = EVeyraClientState::SigningIn;
	/** Increases with every change. */
	uint32 Revision = 0;
	/** The signed-in player's name. */
	FString DisplayName;
	/** A request the player asked for is in flight: intents wait. */
	bool bBusy = false;
	/** The problem on the current screen, if any. */
	TOptional<FVeyraClientProblem> Problem;
	/**
	 * Why the player is here, as a code for the presentation: a cancelled select's reason, such as
	 * "timed_out" or "left"; "you_left" for the player who left it; "connection_lost" or
	 * "join_failed" after a match; or how a match found ended without its select:
	 * "match_found_declined" (the player declined), "match_found_requeued" (not through the
	 * player's party, and it is queued again), or, when the party left the queue,
	 * "match_found_missed" (the player gave no answer) or "match_found_abandoned" (the player
	 * accepted; someone in their party did not). Empty for none.
	 */
	FString Notice;
	/** The signed-in player's account. */
	FString AccountId;
	/**
	 * The player's settings changed here and on another machine since they last matched: they choose
	 * which to keep, "This device" or "Your account" (ADR-024 §1). After sign-in, the flow waits for it.
	 */
	bool bSettingsConflict = false;
	/** SignInFailed: what the launcher was told. */
	TOptional<VeyraLaunchHandshake::EFailure> SignInFailure;
	/** StarterChoice: the starters, in the catalog's order. */
	TArray<FString> Starters;
	/** Shell: the modes the Play screen offers, in the backend's order; empty until read. */
	TArray<VeyraBackendProtocol::FModeInfo> Modes;
	/** Shell and MatchFound: the player's party as last read; unset while they have none, or before it is read. */
	TOptional<VeyraBackendProtocol::FParty> Party;
	/** While the party is in matchmaking: when it entered, on the flow host's clock. */
	double QueuedSince = 0.0;
	/** MatchFound: the match found as last read. */
	VeyraBackendProtocol::FMatchFound MatchFound;
	/** MatchFound: when the acceptance timer ends, on the flow host's clock. */
	double AcceptEndsAt = 0.0;
	/** Selecting: the Vanguards the player may pick, in the catalog's order; empty until read. */
	TArray<FString> AvailableVanguards;
	/** Selecting: every released Vanguard, which a draft's bans may name (ADR-041 §1); empty until read. */
	TArray<FString> ReleasedVanguards;
	/** Selecting: the select as last read. */
	VeyraBackendProtocol::FSelect Select;
	/** Selecting: when the pick timer ends, on the flow host's clock. */
	double PickEndsAt = 0.0;
	/** From MatchStarting on: the player's match. */
	FString MatchId;
	/** Results: the verified result, or unset when none arrived in time. */
	TOptional<VeyraBackendProtocol::FMatchOutcome> Result;
	/** Shell: Match History, once the player opens it. */
	FVeyraMatchHistory History;
	/** Lobby: the player's custom lobby as last read; unset elsewhere. */
	TOptional<VeyraBackendProtocol::FLobby> Lobby;
	/** Shell and Lobby: friends, requests and invitations. */
	FVeyraSocial Social;

	/**
	 * Whether the backend serves custom lobbies: the lobby route's 404 says it does not (ADR-021 §1),
	 * and then no Custom Game is offered.
	 */
	bool bCustomLobbies = true;
};
