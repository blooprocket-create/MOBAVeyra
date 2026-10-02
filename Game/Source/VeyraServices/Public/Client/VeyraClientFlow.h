// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Backend/VeyraBackendTransport.h"
#include "Client/VeyraClientFlowTypes.h"
#include "Client/VeyraClientIntents.h"
#include "Delegates/Delegate.h"
#include "Handoff/VeyraPipeLineReader.h"
#include "Settings/VeyraAccountSettingsSync.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"
#include "VeyraMatchTypes.h"

class UVeyraServicesSettings;

/** What the flow needs from the engine. The game's subsystem provides it; tests fake it. */
class IVeyraClientFlowHost
{
public:
	virtual ~IVeyraClientFlowHost() = default;

	/** Seconds on a clock that only goes forward. */
	virtual double Now() const = 0;

	/** Looks at standard input for the launch code without waiting. */
	virtual EVeyraPipeRead PollLaunchCode(FString& OutLine) = 0;

	/** Writes a launch-handshake line on standard output (ADR-010 §5). */
	virtual void WriteHandshake(const FString& Line) = 0;

	/** Travels to the match server at Address, such as "127.0.0.1:7780", with the join ticket. False if it cannot. */
	virtual bool TravelToMatch(const FString& Address, const FString& Ticket) = 0;

	/** Forgets the join ticket and travels to the front end. */
	virtual void TravelToFrontEnd() = 0;

	/** Quits the game: only ever the player's choice. */
	virtual void QuitGame() = 0;
};

/** The flow's waits and retries, from UVeyraServicesSettings. */
struct FVeyraClientFlowConfig
{
	/** What the game reports when it redeems its launch code. */
	FString BuildVersion;
	double LaunchCodeReadTimeoutSeconds = 0.0;
	int32 RequestAttempts = 0;
	double RetryIntervalSeconds = 0.0;
	double SelectPollIntervalSeconds = 0.0;
	double MatchPollIntervalSeconds = 0.0;
	double MatchWaitTimeoutSeconds = 0.0;
	double ResultPollIntervalSeconds = 0.0;
	double ResultWaitTimeoutSeconds = 0.0;
	double ReconnectPollIntervalSeconds = 0.0;
	double PartyPollIntervalSeconds = 0.0;
	double MatchFoundPollIntervalSeconds = 0.0;
	double LobbyPollIntervalSeconds = 0.0;
	double SocialPollIntervalSeconds = 0.0;
	/** How often chat is read, in every signed-in state but Reconnect-only (ADR-046 §6). */
	double ChatPollIntervalSeconds = 0.0;
	/** How many lines each chat conversation keeps. */
	int32 ChatKeepMessages = 0;
	/** How long the player stays in a match that ended, watching the end, before it leaves for the results (ADR-020 §1). */
	double EndingShowSeconds = 0.0;
	/** When the player's account settings are sent (ADR-024 §1). */
	FVeyraAccountSettingsSyncConfig AccountSettings;

	static VEYRASERVICES_API FVeyraClientFlowConfig FromSettings(const UVeyraServicesSettings& Settings, const FString& BuildVersion);
};

/**
 * The client-state coordinator (ADR-004, ADR-010 §2): where a signed-in game is, what the player may
 * ask for, and every conversation with the backend on the player's behalf.
 *
 * It signs in with the launch code from standard input and keeps the game session in memory only.
 * After sign-in, a live match leads to Reconnect-only, then a select in progress resumes, then a
 * player without a starter chooses one; otherwise the shell. Practice opens a champion select. In
 * the shell the flow also reads the modes and keeps reading the player's party: the leader chooses
 * a matchmade mode, everyone readies up, and the leader queues it. A match found blocks everything
 * else until it is answered: once every player accepts, its champion select opens; otherwise the
 * player returns to the shell, queued again or not as the backend decided. A custom lobby (ADR-021)
 * is its own state, kept up to date by reading it: its host seats humans and bots, sets its rules and
 * starts its champion select, which returns to the lobby if it ends without a match. The shell and the
 * lobby also read the player's friends and invitations, and those reads never stop the flow when they
 * fail. Once every pick is
 * locked the flow waits for the match's server, joins it with the join ticket, and when the match
 * ends travels back to the front end and waits for the backend's verified result. A lost
 * connection leads back through the backend: Reconnect-only while the match still runs, otherwise
 * its result.
 *
 * The presentation observes the snapshot and asks through intents; the flow and the backend decide.
 * A failure shows as the snapshot's problem, with Retry when repeating the step can help. The flow
 * never quits by itself. It logs "VeyraClientFlow: ..." and never logs a credential.
 */
class VEYRASERVICES_API FVeyraClientFlow final : public IVeyraClientIntents
{
public:
	/** Backend and Host must outlive the flow. Callbacks that arrive after it is gone are ignored. */
	FVeyraClientFlow(IVeyraBackendTransport& InBackend, IVeyraClientFlowHost& InHost, FVeyraClientFlowConfig InConfig);
	virtual ~FVeyraClientFlow() override;

	FVeyraClientFlow(const FVeyraClientFlow&) = delete;
	FVeyraClientFlow& operator=(const FVeyraClientFlow&) = delete;

	/**
	 * Tells the launcher the game awaits its launch code and starts reading it. With a problem in
	 * the game's own settings or command line, signing in fails at once with that problem instead.
	 */
	void Start(const FString& ConfigurationProblem = FString());

	/** Reads the launch code and runs the waits that are due. The host calls it every frame. */
	void Tick();

	/**
	 * Keeps the player's account settings in Cache and the backend's copy the same from sign-in on
	 * (ADR-024 §1). Before Start; Cache must outlive the flow. Without it, account settings stay local.
	 */
	void SyncAccountSettings(IVeyraAccountSettingsCache& Cache);

	// IVeyraClientIntents
	virtual const FVeyraClientSnapshot& GetSnapshot() const override { return Snapshot; }
	virtual FSimpleMulticastDelegate& OnChanged() override { return Changed; }
	virtual bool CanIssue(EVeyraClientIntent Intent) const override;
	virtual double GetRemainingPickSeconds() const override;
	virtual double GetQueuedSeconds() const override;
	virtual double GetRemainingAcceptSeconds() const override;
	/** Each intent is refused, returning false, when CanIssue says no or its argument is not on offer. */
	virtual bool ChooseStarter(const FString& VanguardId) override;
	virtual bool StartPractice() override;
	/** Only an enabled, matchmade mode is on offer: the others are not available yet. */
	virtual bool SelectMode(const FString& ModeId) override;
	virtual bool SetReady(bool bReady) override;
	virtual bool FindMatch() override;
	virtual bool CancelQueue() override;
	virtual bool AcceptMatch() override;
	virtual bool DeclineMatch() override;
	virtual bool HoverVanguard(const FString& VanguardId) override;
	virtual bool LockVanguard(const FString& VanguardId) override;
	virtual bool LeaveSelect() override;
	virtual bool ChooseFluxSpell(int32 Slot, const FString& SpellId) override;
	/** Only a released Vanguard not yet banned is on offer. */
	virtual bool HoverBan(const FString& VanguardId) override;
	virtual bool BanVanguard(const FString& VanguardId) override;
	/** Only a locked teammate is on offer. */
	virtual bool OfferTrade(int32 Seat) override;
	/** Only a teammate whose offer stands is on offer. */
	virtual bool AnswerTrade(int32 Seat, bool bAccept) override;
	virtual bool Reconnect() override;
	virtual bool ContinueFromResults() override;
	virtual bool Retry() override;
	virtual bool ResolveSettingsConflict(bool bKeepThisDevice) override;
	virtual bool Quit() override;
	virtual bool LoadHistory(const VeyraBackendProtocol::FHistoryFilter& Filter) override;
	virtual bool LoadMoreHistory() override;
	/** Only a listed match is on offer. */
	virtual bool OpenHistoryMatch(const FString& MatchId) override;
	virtual bool CloseHistoryMatch() override;
	virtual bool CreateLobby() override;
	/** Only one of the player's invitations is on offer. */
	virtual bool AcceptLobbyInvite(const FString& InviteId) override;
	virtual bool DeclineLobbyInvite(const FString& InviteId) override;
	/** Only a friend not already in the lobby is on offer. */
	virtual bool InviteToLobby(const FString& AccountId) override;
	virtual bool LeaveLobby() override;
	virtual bool KickFromLobby(const FString& AccountId) override;
	/** Only another human in the lobby, and an empty seat of it, are on offer. */
	virtual bool MoveInLobby(const FString& AccountId, const FString& Side, int32 Index) override;
	/** Only a seat no human holds, and a Vanguard and difficulty the lobby offers bots, are on offer. */
	virtual bool SetLobbyBot(const FString& Side, int32 Index, const FString& VanguardId, const FString& Difficulty) override;
	virtual bool RemoveLobbyBot(const FString& Side, int32 Index) override;
	virtual bool SetLobbySettings(bool bVictoryEnabled, TOptional<double> StartingGold) override;
	virtual bool LaunchLobby() override;
	virtual bool SendFriendRequest(const FString& DisplayName) override;
	virtual bool AnswerFriendRequest(const FString& AccountId, bool bAccept) override;
	virtual bool RemoveFriend(const FString& AccountId) override;
	virtual bool InviteToParty(const FString& AccountId) override;
	virtual bool AcceptPartyInvite(const FString& InviteId) override;
	virtual bool DeclinePartyInvite(const FString& InviteId) override;
	virtual bool JoinFriendParty(const FString& AccountId) override;
	virtual bool LeaveParty() override;
	virtual bool KickFromParty(const FString& AccountId) override;
	virtual bool TransferPartyLeader(const FString& AccountId) override;
	virtual bool SetPartyPrivacy(VeyraBackendProtocol::EPartyPrivacy Privacy) override;
	virtual bool BlockPlayer(const FString& AccountId) override;
	virtual bool UnblockPlayer(const FString& AccountId) override;
	virtual bool CancelFriendRequest(const FString& AccountId) override;
	virtual bool LoadCollection() override;
	virtual bool PurchaseVanguard(const FString& VanguardId, VeyraBackendProtocol::ECurrency Currency) override;
	/** Only a friend is on offer for a direct message; the select's chat in champion select, the post-match chat on the results screen. */
	virtual bool SendChatMessage(VeyraBackendProtocol::EChatKind Kind, const FString& Target, const FString& Text) override;
	/** Only a friend is on offer. */
	virtual bool OpenDirectChat(const FString& AccountId) override;
	virtual bool CloseDirectChat() override;
	/** Only another participant of the match is on offer. */
	virtual bool MutePostMatchChat(const FString& AccountId, bool bMute) override;
	virtual bool ReportPlayer(const FString& Name, const FString& Reason, const FString& Details) override;
	virtual bool CommendTeammate(const FString& Name) override;
	virtual bool OpenProfile(const FString& Name) override;
	virtual bool CloseProfile() override;
	virtual bool LoadMoreProfileMatches() override;
	virtual bool FilterProfileMatches(const VeyraBackendProtocol::FHistoryFilter& Filter) override;
	virtual bool OpenProfileMatch(const FString& MatchId) override;
	virtual bool CloseProfileMatch() override;
	virtual bool LoadProfileSettings() override;
	virtual bool SaveProfileSettings(const VeyraBackendProtocol::FProfileSettings& Settings) override;

	/** Which intents a state allows at all, before the snapshot's details: a pure table. */
	static bool IsIntentAllowed(EVeyraClientState State, EVeyraClientIntent Intent);

	/** The host loaded a world. */
	void NotifyWorld(EVeyraClientWorld World);

	/** The match's replicated phase changed, or became known. */
	void NotifyMatchPhase(EVeyraMatchPhase Phase);

	/** The connection to a match server failed, or travel to it did. */
	void NotifyConnectionFailed(const FString& Reason);

private:
	enum class EVerb : uint8
	{
		Get,
		Post,
		Put,
		Delete,
	};

	struct FWait
	{
		double At = 0.0;
		uint32 Epoch = 0;
		TFunction<void()> Run;
	};

	using FAnswer = TFunction<void(const FVeyraBackendResponse&)>;

	// Signing in.
	void TickLaunchCode();
	void Redeem(const FString& LaunchCode);
	void OnRedeemed(const FVeyraBackendResponse& Response);
	void FailSignIn(VeyraLaunchHandshake::EFailure Failure, const FString& Reason);

	// Where the player is.
	/** Finds where the player is. Notice is kept for the shell, if that is where they are. */
	void Resume(const FString& Notice = FString());
	void LoadProfile();

	// The shell, the party and its queue.
	void EnterShell(const FString& Notice);
	void LoadModes();
	void PollParty();
	/** Sends a party request; its answer is the party, which is shown unless a later request's answer already was. */
	void CallParty(EVerb Verb, const FString& Path, const FString& Body, const TCHAR* What);
	/** Reads the party once, now, as after a social request changed it; the party's poll goes on as before. */
	void RefreshParty();
	/**
	 * Joins a party from the friends panel, by an invitation or a friend's Public party: a refusal shows in the
	 * panel, the answer is the player's new party. Name is whose party it is.
	 */
	void JoinPartyFromPanel(const FString& Path, const FString& Name);
	/** The member AccountId of the party the player leads before matchmaking, other than the player; null otherwise. */
	const VeyraBackendProtocol::FPartyMember* FindOtherMember(const FString& AccountId) const;
	/** Shows a party read by the request numbered Sequence. False if a later request's answer was already shown. */
	bool ApplyParty(uint32 Sequence, TOptional<VeyraBackendProtocol::FParty> Party);
	/** The party left the shell's hands: into a match found or a champion select. */
	void FollowParty();
	const VeyraBackendProtocol::FModeInfo* FindMode(const FString& ModeId) const;
	bool LeadsIdleParty() const;

	// The custom lobby.
	/** After the profile: into the player's lobby if they are in one, otherwise the shell. Notice is kept for either. */
	void LoadLobby(const FString& Notice);
	void EnterLobby(VeyraBackendProtocol::FLobby Lobby, const FString& Notice);
	void PollLobby();
	/** Sends a lobby request whose answer is the lobby, shown unless a later request's answer already was. */
	void CallLobby(EVerb Verb, const FString& Path, const FString& Body, const TCHAR* What);
	/** Shows a lobby read by the request numbered Sequence. False if a later request's answer was already shown. */
	bool ApplyLobby(uint32 Sequence, TOptional<VeyraBackendProtocol::FLobby> Lobby);
	/** The lobby is in its champion select: into it. */
	void FollowLobby();
	bool HostsOpenLobby() const;
	/** The seat Index of Side in the lobby; null if there is none. */
	const VeyraBackendProtocol::FLobbySeat* FindLobbySeat(const FString& Side, int32 Index) const;

	// Friends and invitations.
	/** Reads them, then again after SocialPollIntervalSeconds, for as long as the state lasts. */
	void PollSocial();
	/** Reads them once, now, as after the player changed them. */
	void ReadSocial(bool bThenPoll);
	/** Sends a social request; a refusal shows in the friends panel, not as the screen's problem. Either way the lists are read again. */
	void CallSocial(EVerb Verb, const FString& Path, const FString& Body, const FString& Name, TFunction<void(const FVeyraBackendResponse&)> OnSuccess);
	/** Shows what came of a social request in the friends panel. */
	void ShowSocialFeedback(const FString& Code, const FString& Name);
	/** Shows lists read by the social read numbered Sequence, unless a later read's were shown already. */
	void ApplySocial(uint32 Sequence, FVeyraSocial Read);

	// Account progression, the Collection and purchases (VeyraClientFlowProgression.cpp; ADR-045 §7).
	/** Reads the account's level and balances once; a failed read keeps the last, and never stops the flow. */
	void ReadProgression();
	/** Shows what came of a purchase in the Collection, not as the screen's problem. */
	void ShowCollectionFeedback(const FString& Code, const FString& VanguardId);

	// Reports and commendation (VeyraClientFlowConduct.cpp; ADR-047).
	/** Opens MatchId's conduct record, empty for none, and reads it once; a failed read offers no report or commendation. */
	void ReadConduct(const FString& MatchId);
	/** Shows what came of a report or commendation beside the player it was about, not as the screen's problem. */
	void ShowConductFeedback(const FString& Code, const FString& Name);

	// Player profiles (VeyraClientFlowProfile.cpp; ADR-048).
	/** Reads Name's profile into the opened view, and its shared Match History's first page when it shares it. */
	void ReadProfile(const FString& Name);
	/** Reads a page of the opened profile's shared Match History from Cursor. */
	void ReadProfileMatches(const FString& Name, const VeyraBackendProtocol::FHistoryFilter& Filter, const FString& Cursor);
	/** Reads the player's own profile as another player sees it, for the Profile page's preview. */
	void ReadProfilePreview();

	// Chat (VeyraClientFlowChat.cpp; ADR-046).
	/** Whether chat is read now: signed in, and in any state but Reconnect-only. */
	bool ChatRuns() const;
	/** Reads chat when it is due and sends messages whose retry is due; on its own clock, so state changes never stop it. */
	void TickChat(double Now);
	void PollChat();
	/** Shows one message the backend delivered or answered with; History marks the first read after sign-in. */
	void ApplyChatMessage(const VeyraBackendProtocol::FChatMessage& Message, bool bHistory);
	/** Where a message of the backend's belongs, or null when the player has no such conversation now. */
	FVeyraChatConversation* ChatConversationFor(const VeyraBackendProtocol::FChatMessage& Message);
	/** Sends the pending message ClientId; a lost answer is tried again with the same ID. */
	void SendPendingChat(const FString& ClientId);
	/** Marks the player's own line ClientId as refused or never answered. */
	void FailChatLine(const FString& ClientId, const FString& Failure);
	/** Drops a conversation's oldest lines beyond ChatKeepMessages. */
	void TrimChat(FVeyraChatConversation& Conversation) const;
	/** A new party, select or match starts its conversation afresh. */
	void ResetChatConversation(FVeyraChatConversation& Conversation, const FString& Key);
	/** The results screen opens on a match: its post-match chat starts unjoined. */
	void StartPostMatchChat(const FString& MatchId);
	/** The player leaves the results screen: the backend is told, and the post-match chat ends for them. */
	void LeavePostMatchChat();
	/**
	 * Sends a request once, for reads that must never stop the flow: an answer that arrives after the
	 * state changed is ignored and a refused session ends it, but anything else, even no answer at all,
	 * goes to OnAnswer.
	 */
	void Probe(EVerb Verb, const FString& Path, FAnswer OnAnswer);
	/** Sends Verb to the backend with the game session. */
	void Send(EVerb Verb, const FString& Path, const FString& Body, FVeyraBackendCallback OnDone);

	// A match found.
	void EnterMatchFound(const VeyraBackendProtocol::FMatchFound& Found);
	void PollMatchFound();
	void ApplyMatchFound(const VeyraBackendProtocol::FMatchFound& Found);
	/** The match found is over: into its select, or back to the shell. */
	void LeaveMatchFound();
	void AnswerMatchFound(bool bAccept);

	// Champion select.
	void EnterSelecting(const VeyraBackendProtocol::FSelect& Select);
	void ApplySelect(const VeyraBackendProtocol::FSelect& Select);
	void LoadAvailableVanguards();
	void PollSelect();
	void LearnHowSelectEnded();
	/**
	 * Sends a select action whose answer is the select. A refusal whose code is in ShownRefusals is
	 * shown; any other means the select moved on, which the next read shows.
	 */
	void SendSelectAction(EVerb Verb, const TCHAR* Path, const FString& Body, const TCHAR* What, TArray<const TCHAR*> ShownRefusals);
	void EnterMatchStarting(const FString& MatchId);
	void PollMatch();
	void Connect(const VeyraBackendProtocol::FMyMatch& Match);
	void LeaveMatch(bool bEnded, const FString& Notice);
	void EnterAwaitingResults();
	void PollResult();
	void ShowResults(TOptional<VeyraBackendProtocol::FMatchOutcome> Outcome);
	/** Results: asks for the result again until its rewards arrive or RewardsDeadline passes. */
	void PollRewards();
	void EnterReconnectOnly(const FString& MatchId);
	void PollReconnect();
	void EndSession();

	/** Enters a state: its waits, problem, notice and busy flag start afresh, and answers to the old state's requests are ignored. */
	void Enter(EVeyraClientState NewState, const FString& Notice = FString());

	/** Runs Run after Seconds, unless the state changes first. */
	void After(double Seconds, TFunction<void()> Run);

	/**
	 * Sends a request with the game session. An answer that arrives after the state changed is
	 * ignored. A refused session ends it. No answer, or a server error, is tried again up to the
	 * configured attempts, then shown as a problem with Retry. Anything else goes to OnAnswer.
	 */
	void Call(EVerb Verb, const FString& Path, const FString& Body, FAnswer OnAnswer, int32 Attempt = 1);

	/** Shows a problem. With Retry set, the Retry intent runs it. */
	void ShowProblem(const FString& Code, const FString& Message, TFunction<void()> RetryStep);

	/** Shows the backend's refusal of a request, by its error code. */
	void ShowRefusal(const FVeyraBackendResponse& Response, const TCHAR* What, TFunction<void()> RetryStep);

	/** Shows an answer that could not be read. */
	void ShowBadAnswer(const TCHAR* What, const FString& Problem, TFunction<void()> RetryStep);

	void SetBusy(bool bBusy);
	void Broadcast();
	void Log(const FString& Message) const;

	IVeyraBackendTransport& Backend;
	IVeyraClientFlowHost& Host;
	FVeyraClientFlowConfig Config;
	/** Expires with the flow, so late callbacks know to do nothing. */
	TSharedRef<bool> Alive;

	FVeyraClientSnapshot Snapshot;
	FSimpleMulticastDelegate Changed;

	/** Increases on every state change; a request or wait from an older state is stale. */
	uint32 Epoch = 0;
	TArray<FWait> Waits;
	TFunction<void()> RetryAction;

	bool bReadingLaunchCode = false;
	double LaunchCodeDeadline = 0.0;
	/** The game session credential ("vgs_"). In memory only. */
	FString GameSession;
	FString SelectId;
	/** Numbers party requests, so an answer overtaken by a later one is not shown. */
	uint32 PartySequence = 0;
	uint32 ShownPartySequence = 0;
	/** The same for lobby requests, and for reads of friends and invitations. */
	uint32 LobbySequence = 0;
	uint32 ShownLobbySequence = 0;
	uint32 SocialSequence = 0;
	uint32 ShownSocialSequence = 0;
	/** Syncs the account settings, when the host gave the flow somewhere to keep them. */
	TUniquePtr<FVeyraAccountSettingsSync> AccountSettings;
	/** The shell's next read of the party says whether a match found that did not go ahead left the party queued. */
	bool bExplainQueue = false;
	double MatchDeadline = 0.0;
	double ResultDeadline = 0.0;
	double RewardsDeadline = 0.0;
	/** The player left their match because it ended, not because the connection failed. */
	bool bMatchEnded = false;
	/** The match ended; the player watches the end before it leaves for the results. */
	bool bWatchingEnd = false;
	/**
	 * A purchase the player asked for whose answer has not arrived. Asking again for the same Vanguard and
	 * currency reuses its ID, so a lost answer never spends twice (Account, Collection & Mastery Bible §6).
	 */
	struct FPendingPurchase
	{
		FString Id;
		FString VanguardId;
		VeyraBackendProtocol::ECurrency Currency = VeyraBackendProtocol::ECurrency::Flux;
	};
	TOptional<FPendingPurchase> PendingPurchase;

	/** A report whose answer has not arrived; reporting the same player in the same match again reuses its ID. */
	struct FPendingReport
	{
		FString Id;
		FString MatchId;
		FString Name;
	};
	TOptional<FPendingReport> PendingReport;

	/** A chat message the player sent whose answer has not arrived; a lost answer is sent again with its ID. */
	struct FChatSend
	{
		FString ClientId;
		VeyraBackendProtocol::EChatKind Kind = VeyraBackendProtocol::EChatKind::Party;
		/** The friend of a direct message, or the match of a post-match one. */
		FString Target;
		FString Text;
		int32 Attempt = 1;
		/** When the next try is due; 0 while one is in flight. */
		double DueAt = 0.0;
	};
	TArray<FChatSend> ChatSends;
	/** Chat's own clock: the next read, and whether one is in flight. */
	double NextChatPollAt = 0.0;
	bool bChatPollInFlight = false;
	/** The cursor of the next read; before the first read there is none, and the backend answers with history. */
	bool bChatHasCursor = false;
	int64 ChatCursor = 0;
	/** Increases when the game session ends, so a chat answer for the old session is ignored. */
	uint32 ChatGeneration = 0;
};
