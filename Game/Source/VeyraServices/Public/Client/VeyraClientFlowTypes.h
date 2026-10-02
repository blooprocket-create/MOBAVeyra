// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Backend/VeyraBackendProtocol.h"
#include "Backend/VeyraChatProtocol.h"
#include "Backend/VeyraConductProtocol.h"
#include "Backend/VeyraProfileProtocol.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
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
	/** Considers a ban in the player's draft ban turn, which their team sees (ADR-042 §1). */
	HoverBan,
	/** Bans a Vanguard for the player's side in their draft ban turn. */
	BanVanguard,
	/** Offers a locked teammate the player's locked Vanguard for theirs (ADR-042 §2). */
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
	/** Invites a friend into the player's party, making a mode-less one if they have none (ADR-044 §1; UX-9). Any member's. */
	InviteToParty,
	/** Joins the party an invitation is from, leaving the player's own, which must be idle. */
	AcceptPartyInvite,
	DeclinePartyInvite,
	/** Joins a friend's Public party without an invitation (ADR-044 §3). */
	JoinFriendParty,
	/** Leaves the party. Leaving a queued party takes it out of the queue (Parties & Social Bible §2). */
	LeaveParty,
	/** The leader's, before matchmaking: removes another member (§1; UX-10). */
	KickFromParty,
	/** The leader's, before matchmaking: hands leadership to another member, once the player confirmed it (UX-11). */
	TransferPartyLeader,
	/** The leader's: makes the party Public or Private (§1). */
	SetPartyPrivacy,
	/** Blocks a friend, or a player who asked to be friends (§6). */
	BlockPlayer,
	UnblockPlayer,
	/** Withdraws a friend request the player sent. */
	CancelFriendRequest,
	/** Keeps this device's settings or the account's, when both changed (ADR-024 §1). Whenever the choice shows. */
	ResolveSettingsConflict,
	/** Reads the Collection: every released Vanguard, with the player's ownership and Mastery (ADR-045 §7). */
	LoadCollection,
	/** Buys a Vanguard with one account currency, once the player confirmed its price (ADR-045 §6). */
	PurchaseVanguard,
	/**
	 * Sends a chat message (ADR-046): to the party or a friend wherever the player is signed in, except
	 * Reconnect-only; to the team in champion select; across both teams on the results screen.
	 */
	SendChatMessage,
	/** Shows one friend's direct conversation in the sidebar, or none. */
	OpenDirectChat,
	CloseDirectChat,
	/** Mutes or unmutes another participant in the results screen's post-match chat, for the player only. */
	MutePostMatchChat,
	/** Reports another human participant of the results' match, or of an opened Match History record (ADR-047 §2). */
	ReportPlayer,
	/** Commends a teammate of the results' match, once (ADR-047 §3). */
	CommendTeammate,
	/** Opens a player's profile by name: from the friends card, the player menu or the Profile page (ADR-048 §5). */
	OpenProfile,
	CloseProfile,
	/** Reads the opened profile's next page of shared Match History. */
	LoadMoreProfileMatches,
	/** Opens one of the opened profile's shared matches into its report. */
	OpenProfileMatch,
	CloseProfileMatch,
	/** Reads the player's own profile choices, the catalog and their profile as others see it. */
	LoadProfileSettings,
	/** Saves the player's profile choices (ADR-048 §4). */
	SaveProfileSettings,
	/** Reads the player's display name and what changing it takes (ADR-049). */
	LoadDisplayName,
	/** Changes the player's display name; free first and when another player claimed it, paid later. */
	ChangeDisplayName,
	/**
	 * Leaves the live match on purpose (ADR-053 §1): as a disconnect does, the Vanguard plays on under autopilot, and the
	 * player may reconnect to it from Reconnect-only. In a match only.
	 */
	LeaveMatch,
	/** Dismisses the break reminder after a match (ADR-053 §4). The results and the shell. */
	DismissPlayReminder,
};

/** Which kind of world the client just loaded. */
enum class EVeyraClientWorld : uint8
{
	/** The front end, or any world not connected to a server. */
	FrontEnd,
	/** A world connected to a match server. */
	Match,
};

/** Results: whether the flow still waits for the result's rewards (ADR-045 §7). */
enum class EVeyraRewardsWait : uint8
{
	/** The rewards arrived with the result, or there are none to wait for. */
	None,
	/** The result arrived without them; the flow asks again. */
	Pending,
	/** They did not arrive within the wait; the account still receives them once they are counted. */
	Late,
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
 * The player's friends, friend requests, invitations and blocks as last read (Parties & Social Bible §1,
 * §6), in the shell and the lobby. Reading them never stops the flow: a failed read is tried again later.
 */
struct FVeyraSocial
{
	/** Whether they have been read at all. */
	bool bLoaded = false;
	VeyraBackendProtocol::FFriends Friends;
	TArray<VeyraBackendProtocol::FLobbyInvite> LobbyInvites;
	/** Invitations into another player's party (ADR-044 §1). */
	TArray<VeyraBackendProtocol::FPartyInvite> PartyInvites;
	/** The players the player blocked (ADR-044 §4). */
	TArray<VeyraBackendProtocol::FAccount> Blocked;
	/**
	 * What came of the player's last social request, for the friends panel rather than the screen's
	 * problem: "friend_requested", "friend_added", "party_invited", "player_blocked" and the like, or the
	 * backend's refusal, such as "account_not_found" or "party_full". Empty for none.
	 */
	FString Feedback;
	/** The name that request was for. */
	FString FeedbackName;
};

/**
 * The player's Collection as last read (Account, Collection & Mastery Bible §4): every released Vanguard,
 * owned or not, with the player's Mastery. Seeing one is never permission to pick it.
 */
struct FVeyraCollection
{
	/** Whether it has been read at all. */
	bool bLoaded = false;
	/** In the catalog's order. */
	TArray<VeyraBackendProtocol::FCollectionEntry> Vanguards;
	/**
	 * What came of the player's last purchase, for the Collection rather than the screen's problem:
	 * "vanguard_purchased", or the backend's refusal, such as "insufficient_balance" or "already_owned".
	 * Empty for none.
	 */
	FString Feedback;
	/** The Vanguard that purchase was for. */
	FString FeedbackVanguard;
};

/** One line of a chat conversation as the player sees it (ADR-046 §6). */
struct FVeyraChatEntry
{
	/** The backend's sequence, which orders every line; 0 while the player's own message awaits its answer. */
	int64 Seq = 0;
	VeyraBackendProtocol::EChatKind Kind = VeyraBackendProtocol::EChatKind::Party;
	FString SenderId;
	FString SenderName;
	/** A direct message's other account: the friend the player talks with, whoever sent it. */
	FString With;
	FString Text;
	/** The sender's own ID for the message. */
	FString ClientId;
	/** The player's own message, sent and not yet answered. */
	bool bPending = false;
	/**
	 * The player's own message that did not go: the backend's refusal, such as "rate_limited" or
	 * "not_friends", or "not_sent" when no answer came. Empty for a sent message.
	 */
	FString Failure;
	/** When it arrived, on the flow host's clock: the match HUD fades lines after a while. */
	double ArrivedAt = 0.0;
	/** It came with the first read after sign-in: history rather than news, so it raises no unread count. */
	bool bHistory = false;
};

/** One conversation's lines (ADR-046 §2). */
struct FVeyraChatConversation
{
	/**
	 * The backend's key: the party, the select and side, or the match. A direct conversation's is the
	 * friend's account.
	 */
	FString Key;
	/** Oldest first, at most the flow's ChatKeepMessages. */
	TArray<FVeyraChatEntry> Lines;
	/** Lines from the friend that arrived while the conversation was not open. Direct conversations only. */
	int32 Unread = 0;
};

/**
 * Party Chat, friend direct messages, champion-select team chat and post-match chat as the player has read
 * them (ADR-046). The backend decides who reads each line; reading never stops the flow.
 */
struct FVeyraChat
{
	/** Whether the first read after sign-in has come. */
	bool bLoaded = false;
	/** The player's current party; a new party starts it afresh. */
	FVeyraChatConversation Party;
	/** By the friend's account. */
	TMap<FString, FVeyraChatConversation> Direct;
	/** The player's side in the current champion select. */
	FVeyraChatConversation Select;
	/** The results screen's cross-team chat for the match just played. */
	FVeyraChatConversation PostMatch;
	/** The player opted into the post-match chat by sending; before that it shows nothing (UX-59). */
	bool bPostMatchJoined = false;
	/** The participants the player muted in the post-match chat. */
	TArray<FString> PostMatchMuted;
	/** The direct conversation the sidebar shows; empty for none. */
	FString OpenDirect;
	/** The friend who sent the latest direct message, whom the match HUD's reply command answers. */
	FString LastDirectFrom;
};

/** Everything the presentation shows about the flow. Only the flow changes it. */
/**
 * Results, or an opened Match History record: the player's own reports and commendation in that match, and
 * what a report may give (ADR-047 §4). Reports and commendation wait until it is read.
 */
struct FVeyraConduct
{
	/** The match it is of; empty while none is open. */
	FString MatchId;
	/** Whether the record has been read; a backend without reports never reads one. */
	bool bLoaded = false;
	VeyraBackendProtocol::FConductRecord Record;
	/**
	 * What came of the player's last report or commendation: "report_sent", "commended", or the backend's
	 * refusal, such as "commend_closed". Empty for none.
	 */
	FString Feedback;
	/** Whom that was about. */
	FString FeedbackName;
};

/**
 * A player's profile the client shows (ADR-048 §5), opened by name; the name is empty while none is open. A
 * profile shares its Match History only while its owner chooses to (UX-72).
 */
struct FVeyraProfileView
{
	FString Name;
	/** Whether it has been read. */
	bool bLoaded = false;
	/** An unknown name, or a block either way: the screen says only that the profile is unavailable. */
	bool bUnavailable = false;
	VeyraBackendProtocol::FPublicProfile Profile;
	/** Its shared Match History as read, newest first, with the next page's cursor; read once the profile is. */
	bool bMatchesLoaded = false;
	TArray<VeyraBackendProtocol::FHistoryEntry> Matches;
	FString Next;
	/** One shared match opened into its report; unset while none is. */
	TOptional<VeyraBackendProtocol::FMatchOutcome> OpenedMatch;
};

/** The player's own profile choices, the catalog, and their profile as others see it (ADR-048 §4); read on the Profile page. */
struct FVeyraProfileSettings
{
	bool bLoaded = false;
	VeyraBackendProtocol::FProfileSettings Saved;
	VeyraBackendProtocol::FProfileCatalog Catalog;
	/** The player's profile as another player sees it, read with the choices and again after a save. */
	TOptional<VeyraBackendProtocol::FPublicProfile> Preview;
	/** What came of the last save: "profile_saved", or the backend's refusal, such as "not_owned". Empty for none. */
	FString Feedback;
};

/** The player's display name and what changing it takes (ADR-049), read on the Profile page. */
struct FVeyraDisplayName
{
	bool bLoaded = false;
	VeyraBackendProtocol::FDisplayNameStatus Status;
	/** What came of the last change: "name_changed", or the backend's refusal, such as "display_name_taken". Empty for none. */
	FString Feedback;
};

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
	/** After a match: how long the player had played in a row when they left it, for the break reminder; 0 once dismissed (ADR-053 §4). */
	double PlayedSeconds = 0.0;
	/** MatchFound: when the acceptance timer ends, on the flow host's clock. */
	double AcceptEndsAt = 0.0;
	/** Selecting: the Vanguards the player may pick, in the catalog's order; empty until read. */
	TArray<FString> AvailableVanguards;
	/** Selecting: every released Vanguard, which a draft's bans may name (ADR-042 §1); empty until read. */
	TArray<FString> ReleasedVanguards;
	/** Selecting: the select as last read. */
	VeyraBackendProtocol::FSelect Select;
	/** Selecting: when the pick timer ends, on the flow host's clock. */
	double PickEndsAt = 0.0;
	/** From MatchStarting on: the player's match. */
	FString MatchId;
	/** Results: the verified result, or unset when none arrived in time. */
	TOptional<VeyraBackendProtocol::FMatchOutcome> Result;
	/** Results: whether the result's rewards are still to come. */
	EVeyraRewardsWait RewardsWait = EVeyraRewardsWait::None;
	/** Shell: Match History, once the player opens it. */
	FVeyraMatchHistory History;
	/** Lobby: the player's custom lobby as last read; unset elsewhere. */
	TOptional<VeyraBackendProtocol::FLobby> Lobby;
	/** Shell and Lobby: friends, requests and invitations. */
	FVeyraSocial Social;
	/**
	 * The account's level and balances as last read (ADR-045 §7): read on entering the shell and after a
	 * purchase; unset before the first read, or from a backend without progression. A failed read keeps it.
	 */
	TOptional<VeyraBackendProtocol::FProgression> Progression;
	/** Shell: the Collection, once the player opens it. */
	FVeyraCollection Collection;
	/** Results, or an opened Match History record: the player's reports and commendation in that match. */
	FVeyraConduct Conduct;
	/** A player's profile opened from the friends card, the player menu or the Profile page. */
	FVeyraProfileView ProfileView;
	/** Shell: the player's own profile choices, once the Profile page opens. */
	FVeyraProfileSettings ProfileSettings;
	/** Shell: the player's display name and what changing it takes, once the Profile page opens. */
	FVeyraDisplayName DisplayNameChange;
	/**
	 * Another player claimed this player's name while they were away: they choose a new one, for free, before
	 * anything else (ADR-049 §4). Read with the profile on sign-in.
	 */
	bool bRenameRequired = false;
	/** Party, direct, select and post-match chat, read in every signed-in state but Reconnect-only (ADR-046 §6). */
	FVeyraChat Chat;

	/**
	 * Whether the backend serves custom lobbies: the lobby route's 404 says it does not (ADR-021 §1),
	 * and then no Custom Game is offered.
	 */
	bool bCustomLobbies = true;
};
