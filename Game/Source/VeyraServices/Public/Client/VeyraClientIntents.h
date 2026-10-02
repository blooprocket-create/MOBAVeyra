// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Client/VeyraClientFlowTypes.h"
#include "Delegates/Delegate.h"

/**
 * What presentation may do with the client-state coordinator (ADR-010 §2): observe its snapshot and
 * ask through intents. Whether an intent is allowed now is CanIssue's to say; the coordinator and the
 * backend decide the outcome. Each intent returns false when it was refused outright.
 */
class IVeyraClientIntents
{
public:
	virtual ~IVeyraClientIntents() = default;

	virtual const FVeyraClientSnapshot& GetSnapshot() const = 0;

	/** Broadcast after every change to the snapshot. */
	virtual FSimpleMulticastDelegate& OnChanged() = 0;

	virtual bool CanIssue(EVeyraClientIntent Intent) const = 0;

	/** Seconds left on the select's pick timer, by the backend's clock as last read. */
	virtual double GetRemainingPickSeconds() const = 0;

	/** Seconds the party has been in matchmaking, by the backend's clock as last read; 0 when it is not. */
	virtual double GetQueuedSeconds() const = 0;

	/** Seconds left to accept a match found, by the backend's clock as last read. */
	virtual double GetRemainingAcceptSeconds() const = 0;

	virtual bool ChooseStarter(const FString& VanguardId) = 0;
	virtual bool StartPractice() = 0;
	virtual bool SelectMode(const FString& ModeId) = 0;
	virtual bool SetReady(bool bReady) = 0;
	virtual bool FindMatch() = 0;
	virtual bool CancelQueue() = 0;
	virtual bool AcceptMatch() = 0;
	virtual bool DeclineMatch() = 0;
	virtual bool HoverVanguard(const FString& VanguardId) = 0;
	virtual bool LockVanguard(const FString& VanguardId) = 0;
	virtual bool LeaveSelect() = 0;
	/**
	 * Puts SpellId, a roster Flux Spell or empty for none, in spell slot Slot (from 0). Choosing the
	 * other slot's spell swaps the two.
	 */
	virtual bool ChooseFluxSpell(int32 Slot, const FString& SpellId) = 0;
	/** Considers banning VanguardId, any released Vanguard, in the player's draft ban turn (ADR-042 §1). */
	virtual bool HoverBan(const FString& VanguardId) = 0;
	virtual bool BanVanguard(const FString& VanguardId) = 0;
	/** Offers the locked teammate in seat Seat, an index into the select's seats, the player's locked Vanguard for theirs (ADR-042 §2). */
	virtual bool OfferTrade(int32 Seat) = 0;
	/** Accepts or declines the trade the teammate in seat Seat offers. */
	virtual bool AnswerTrade(int32 Seat, bool bAccept) = 0;
	virtual bool Reconnect() = 0;
	/** Leaves the live match on purpose: the Vanguard plays on, and the player may reconnect (ADR-053 §1). */
	virtual bool LeaveLiveMatch() = 0;
	/** Dismisses the break reminder, which counts the player's play again from now (ADR-053 §4). */
	virtual bool DismissPlayReminder() = 0;
	virtual bool ContinueFromResults() = 0;
	virtual bool Retry() = 0;
	/** Keeps this device's settings, sending them over the account's, or takes the account's (ADR-024 §1). */
	virtual bool ResolveSettingsConflict(bool bKeepThisDevice) = 0;
	virtual bool Quit() = 0;
	/** Reads Match History's first page with Filter, replacing what was read. */
	virtual bool LoadHistory(const VeyraBackendProtocol::FHistoryFilter& Filter) = 0;
	/** Reads the next page, after those read. */
	virtual bool LoadMoreHistory() = 0;
	/** Opens MatchId, one of the listed matches, into its verified result. */
	virtual bool OpenHistoryMatch(const FString& MatchId) = 0;
	virtual bool CloseHistoryMatch() = 0;

	/** Opens a custom lobby the player hosts, seated first on side A (ADR-021). */
	virtual bool CreateLobby() = 0;
	/** Joins the lobby of InviteId, one of the player's invitations. */
	virtual bool AcceptLobbyInvite(const FString& InviteId) = 0;
	virtual bool DeclineLobbyInvite(const FString& InviteId) = 0;
	/** Invites AccountId, one of the player's friends, into the lobby. */
	virtual bool InviteToLobby(const FString& AccountId) = 0;
	virtual bool LeaveLobby() = 0;
	virtual bool KickFromLobby(const FString& AccountId) = 0;
	/** Puts AccountId, a human in the lobby, in the empty seat Index of Side ("A" or "B"). */
	virtual bool MoveInLobby(const FString& AccountId, const FString& Side, int32 Index) = 0;
	/** Puts a bot playing VanguardId at Difficulty in a seat: an empty one, or one a bot holds. */
	virtual bool SetLobbyBot(const FString& Side, int32 Index, const FString& VanguardId, const FString& Difficulty) = 0;
	virtual bool RemoveLobbyBot(const FString& Side, int32 Index) = 0;
	/** Sets the session's rules; an unset StartingGold plays the game's own. */
	virtual bool SetLobbySettings(bool bVictoryEnabled, TOptional<double> StartingGold) = 0;
	virtual bool LaunchLobby() = 0;
	/** Asks the player whose display name is exactly DisplayName to be friends. */
	virtual bool SendFriendRequest(const FString& DisplayName) = 0;
	/** Accepts or declines the friend request from AccountId. */
	virtual bool AnswerFriendRequest(const FString& AccountId, bool bAccept) = 0;
	virtual bool RemoveFriend(const FString& AccountId) = 0;

	// The party and the social panel (ADR-044).

	/** Invites AccountId, one of the player's friends not in their party, into it. */
	virtual bool InviteToParty(const FString& AccountId) = 0;
	/** Joins the party of InviteId, one of the player's party invitations. */
	virtual bool AcceptPartyInvite(const FString& InviteId) = 0;
	virtual bool DeclinePartyInvite(const FString& InviteId) = 0;
	/** Joins the Public party of AccountId, a friend whose party the friends list offers. */
	virtual bool JoinFriendParty(const FString& AccountId) = 0;
	virtual bool LeaveParty() = 0;
	/** Removes AccountId, another member, from the party the player leads. */
	virtual bool KickFromParty(const FString& AccountId) = 0;
	/** Makes AccountId, another member, leader of the party the player leads. The screen asks the player to confirm first. */
	virtual bool TransferPartyLeader(const FString& AccountId) = 0;
	virtual bool SetPartyPrivacy(VeyraBackendProtocol::EPartyPrivacy Privacy) = 0;
	/** Blocks AccountId: a friend, or a player whose friend request waits. */
	virtual bool BlockPlayer(const FString& AccountId) = 0;
	virtual bool UnblockPlayer(const FString& AccountId) = 0;
	/** Withdraws the friend request the player sent AccountId. */
	virtual bool CancelFriendRequest(const FString& AccountId) = 0;
	/** Reads the Collection: every released Vanguard, with the player's ownership and Mastery of each. */
	virtual bool LoadCollection() = 0;
	/** Buys VanguardId with Currency. The screen asks the player to confirm the price first. */
	virtual bool PurchaseVanguard(const FString& VanguardId, VeyraBackendProtocol::ECurrency Currency) = 0;
	/**
	 * Sends Text to a conversation of Kind (ADR-046): the party, the friend Target, the player's side in
	 * champion select, or the results screen's post-match chat, whose first message opts the player in.
	 * The backend decides; a refusal marks the line, never the screen.
	 */
	virtual bool SendChatMessage(VeyraBackendProtocol::EChatKind Kind, const FString& Target, const FString& Text) = 0;
	/** Shows the direct conversation with AccountId, a friend, in the sidebar; its unread count clears. */
	virtual bool OpenDirectChat(const FString& AccountId) = 0;
	virtual bool CloseDirectChat() = 0;
	/** Mutes or unmutes AccountId, another participant, in the post-match chat, for the player only. */
	virtual bool MutePostMatchChat(const FString& AccountId, bool bMute) = 0;
	/**
	 * Reports Name, another human in the match whose conduct record is read, for Reason, one the record offers,
	 * with optional Details (ADR-047 §2). The answer only says the report was received.
	 */
	virtual bool ReportPlayer(const FString& Name, const FString& Reason, const FString& Details) = 0;
	/** Commends Name, a teammate in the results' match; one per match (ADR-047 §3). */
	virtual bool CommendTeammate(const FString& Name) = 0;
	/** Opens Name's profile, the player's own included; a block either way shows it as unavailable (ADR-048 §3). */
	virtual bool OpenProfile(const FString& Name) = 0;
	virtual bool CloseProfile() = 0;
	/** Reads the opened profile's next page of shared Match History. */
	virtual bool LoadMoreProfileMatches() = 0;
	/** Reads the opened profile's shared Match History again from its first page, with Filter. */
	virtual bool FilterProfileMatches(const VeyraBackendProtocol::FHistoryFilter& Filter) = 0;
	/** Opens MatchId, listed in the opened profile's shared Match History, into its report. */
	virtual bool OpenProfileMatch(const FString& MatchId) = 0;
	virtual bool CloseProfileMatch() = 0;
	/** Reads the player's own profile choices, the catalog, and their profile as others see it. */
	virtual bool LoadProfileSettings() = 0;
	/** Saves the player's profile choices: catalog entries, and a featured Vanguard they permanently own or none. */
	virtual bool SaveProfileSettings(const VeyraBackendProtocol::FProfileSettings& Settings) = 0;
	/** Reads the player's display name and what changing it takes. */
	virtual bool LoadDisplayName() = 0;
	/**
	 * Changes the player's display name to Name. Currency ("flux" or "refinedFlux") pays for a voluntary change after
	 * the free one; a required rename is free. The backend decides; a refusal shows beside the name.
	 */
	virtual bool ChangeDisplayName(const FString& Name, const FString& Currency) = 0;
};
