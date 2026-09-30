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
	 * other slot's spell swaps the two, as League's picker does.
	 */
	virtual bool ChooseFluxSpell(int32 Slot, const FString& SpellId) = 0;
	virtual bool Reconnect() = 0;
	virtual bool ContinueFromResults() = 0;
	virtual bool Retry() = 0;
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
};
