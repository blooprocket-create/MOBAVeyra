// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Client/VeyraClientFlow.h"

#include "Backend/VeyraBackendProtocol.h"
#include "Slots/VeyraAbilitySlot.h"
#include "Tuning/VeyraMatchTuningSubsystem.h"
#include "VeyraServicesLog.h"
#include "VeyraServicesSettings.h"

namespace
{
	using VeyraBackendProtocol::EPartyStatus;
	using VeyraBackendProtocol::ESelectState;
	using VeyraLaunchHandshake::EFailure;

	// The player routes (ADR-007 §10, ADR-010 §6–10).
	const TCHAR* const RedeemPath = TEXT("/v1/game-sessions");
	const TCHAR* const MyMatchPath = TEXT("/v1/me/match");
	const TCHAR* const MySelectPath = TEXT("/v1/me/select");
	const TCHAR* const ProfilePath = TEXT("/v1/me/profile");
	const TCHAR* const VanguardsPath = TEXT("/v1/me/vanguards");
	const TCHAR* const StarterPath = TEXT("/v1/me/starter");
	const TCHAR* const PracticePath = TEXT("/v1/practice");
	const TCHAR* const HoverPath = TEXT("/v1/me/select/hover");
	const TCHAR* const LockPath = TEXT("/v1/me/select/lock");
	const TCHAR* const FluxSpellsPath = TEXT("/v1/me/select/spells");
	const TCHAR* const LeaveSelectPath = TEXT("/v1/me/select/leave");
	// A draft's bans and the trades between locked teammates (ADR-042).
	const TCHAR* const BanHoverPath = TEXT("/v1/me/select/ban/hover");
	const TCHAR* const BanPath = TEXT("/v1/me/select/ban");
	const TCHAR* const TradePath = TEXT("/v1/me/select/trade");
	const TCHAR* const AcceptTradePath = TEXT("/v1/me/select/trade/accept");
	const TCHAR* const DeclineTradePath = TEXT("/v1/me/select/trade/decline");
	const TCHAR* const ModesPath = TEXT("/v1/modes");
	const TCHAR* const PartyPath = TEXT("/v1/party");
	const TCHAR* const PartyModePath = TEXT("/v1/party/mode");
	const TCHAR* const PartyReadyPath = TEXT("/v1/party/ready");
	const TCHAR* const QueuePath = TEXT("/v1/party/queue");
	const TCHAR* const MatchFoundPath = TEXT("/v1/me/match-found");
	const TCHAR* const AcceptMatchPath = TEXT("/v1/me/match-found/accept");
	const TCHAR* const DeclineMatchPath = TEXT("/v1/me/match-found/decline");
	// The custom lobby (ADR-021) and the social routes (Parties & Social Bible §1).
	const TCHAR* const LobbyPath = TEXT("/v1/lobby");
	const TCHAR* const LeaveLobbyPath = TEXT("/v1/lobby/leave");
	const TCHAR* const LobbySettingsPath = TEXT("/v1/lobby/settings");
	const TCHAR* const LaunchLobbyPath = TEXT("/v1/lobby/launch");
	const TCHAR* const LobbyInvitesPath = TEXT("/v1/lobby/invites");
	const TCHAR* const FriendsPath = TEXT("/v1/friends");
	const TCHAR* const FriendRequestsPath = TEXT("/v1/friends/requests");

	/** The kinds of champion select matchmaking opens, which a player may leave, and in which locked teammates may trade. */
	const TCHAR* const CasualSelectKind = TEXT("casual");
	const TCHAR* const DraftSelectKind = TEXT("draft");
	/** A custom lobby's champion select, which a player may leave too, and which returns to the lobby. */
	const TCHAR* const CustomSelectKind = TEXT("custom");

	// Why the player is back in the shell or the lobby: they left a custom select, ending it for
	// everyone, or they are no longer in their lobby (the host removed them, or it closed).
	const TCHAR* const YouLeftCustomNotice = TEXT("you_left_custom");
	const TCHAR* const LobbyGoneNotice = TEXT("lobby_gone");

	// What came of a social request, for the friends panel.
	const TCHAR* const FriendRequestedFeedback = TEXT("friend_requested");
	const TCHAR* const FriendAddedFeedback = TEXT("friend_added");
	const TCHAR* const LobbyInvitedFeedback = TEXT("lobby_invited");
	const TCHAR* const SelfFeedback = TEXT("cannot_target_self");
	/** The backend's outcome of a friend request to a player who had already asked. */
	const TCHAR* const BecameFriendsOutcome = TEXT("friends");

	/** The HTTP status of a route the backend does not serve, as when custom lobbies are switched off. */
	constexpr int32 NotFoundStatus = 404;

	FString LobbyMemberPath(const FString& AccountId)
	{
		return TEXT("/v1/lobby/members/") + AccountId;
	}

	FString LobbyInvitePath(const FString& InviteId, const TCHAR* Answer)
	{
		return FString::Printf(TEXT("/v1/lobby/invites/%s/%s"), *InviteId, Answer);
	}

	FString FriendRequestPath(const FString& AccountId, const TCHAR* Answer)
	{
		return FString::Printf(TEXT("/v1/friends/requests/%s/%s"), *AccountId, Answer);
	}

	/** Whether the select is matchmade: Casual Select or Draft Pick. */
	bool IsMatchmade(const VeyraBackendProtocol::FSelect& Select)
	{
		return Select.Kind == CasualSelectKind || Select.Kind == DraftSelectKind;
	}

	/** Whether the select is one a player may leave: a matchmade one, or a custom lobby's. */
	bool IsLeavable(const VeyraBackendProtocol::FSelect& Select)
	{
		return IsMatchmade(Select) || Select.Kind == CustomSelectKind;
	}

	/** Whether the player has locked, in a matchmade select still picking: what a trade needs on each side (ADR-042 §2). */
	const VeyraBackendProtocol::FSelectSeat* LockedTrader(const VeyraBackendProtocol::FSelect& Select)
	{
		const VeyraBackendProtocol::FSelectSeat* You = Select.FindYou();
		return IsMatchmade(Select) && Select.State == VeyraBackendProtocol::ESelectState::Picking && You && !You->Locked.IsEmpty() ? You : nullptr;
	}
	/** A match found's state while it waits for answers, and a player's answer before they give one. */
	const TCHAR* const PendingAnswer = TEXT("pending");
	const TCHAR* const AcceptedAnswer = TEXT("accepted");
	const TCHAR* const DeclinedAnswer = TEXT("declined");

	// How a match found that did not go ahead is explained. Missed and abandoned are refined by the
	// shell's first read of the party: a party back in the queue was not at fault.
	const TCHAR* const DeclinedNotice = TEXT("match_found_declined");
	const TCHAR* const MissedNotice = TEXT("match_found_missed");
	const TCHAR* const AbandonedNotice = TEXT("match_found_abandoned");
	const TCHAR* const RequeuedNotice = TEXT("match_found_requeued");

	/** Whether the player leads the party. */
	bool Leads(const FVeyraClientSnapshot& Snapshot)
	{
		const VeyraBackendProtocol::FPartyMember* You = Snapshot.Party.IsSet() ? Snapshot.Party->Find(Snapshot.AccountId) : nullptr;
		return You && You->bLeader;
	}

	FString SelectPath(const FString& SelectId)
	{
		return TEXT("/v1/me/selects/") + SelectId;
	}

	FString MatchOutcomePath(const FString& MatchId)
	{
		return TEXT("/v1/me/matches/") + MatchId;
	}

	/** The backend's error code, or "http_<status>" when it gave none. */
	FString RefusalCode(const FVeyraBackendResponse& Response)
	{
		const FString Code = VeyraBackendProtocol::ParseErrorCode(Response.Body);
		return Code.IsEmpty() ? FString::Printf(TEXT("http_%d"), Response.Status) : Code;
	}

	bool IsRefusal(const FVeyraBackendResponse& Response, const TCHAR* Code)
	{
		return !Response.IsSuccess() && VeyraBackendProtocol::ParseErrorCode(Response.Body).Equals(Code, ESearchCase::CaseSensitive);
	}
}

const TCHAR* LexToString(EVeyraClientState State)
{
	switch (State)
	{
	case EVeyraClientState::SigningIn:
		return TEXT("SigningIn");
	case EVeyraClientState::SignInFailed:
		return TEXT("SignInFailed");
	case EVeyraClientState::Loading:
		return TEXT("Loading");
	case EVeyraClientState::StarterChoice:
		return TEXT("StarterChoice");
	case EVeyraClientState::Shell:
		return TEXT("Shell");
	case EVeyraClientState::Lobby:
		return TEXT("Lobby");
	case EVeyraClientState::MatchFound:
		return TEXT("MatchFound");
	case EVeyraClientState::Selecting:
		return TEXT("Selecting");
	case EVeyraClientState::MatchStarting:
		return TEXT("MatchStarting");
	case EVeyraClientState::Connecting:
		return TEXT("Connecting");
	case EVeyraClientState::InMatch:
		return TEXT("InMatch");
	case EVeyraClientState::Returning:
		return TEXT("Returning");
	case EVeyraClientState::AwaitingResults:
		return TEXT("AwaitingResults");
	case EVeyraClientState::Results:
		return TEXT("Results");
	case EVeyraClientState::ReconnectOnly:
		return TEXT("ReconnectOnly");
	case EVeyraClientState::SessionEnded:
		return TEXT("SessionEnded");
	}
	return TEXT("Unknown");
}

const TCHAR* LexToString(EVeyraClientIntent Intent)
{
	switch (Intent)
	{
	case EVeyraClientIntent::ChooseStarter:
		return TEXT("ChooseStarter");
	case EVeyraClientIntent::StartPractice:
		return TEXT("StartPractice");
	case EVeyraClientIntent::SelectMode:
		return TEXT("SelectMode");
	case EVeyraClientIntent::SetReady:
		return TEXT("SetReady");
	case EVeyraClientIntent::FindMatch:
		return TEXT("FindMatch");
	case EVeyraClientIntent::CancelQueue:
		return TEXT("CancelQueue");
	case EVeyraClientIntent::AcceptMatch:
		return TEXT("AcceptMatch");
	case EVeyraClientIntent::DeclineMatch:
		return TEXT("DeclineMatch");
	case EVeyraClientIntent::HoverVanguard:
		return TEXT("HoverVanguard");
	case EVeyraClientIntent::LockVanguard:
		return TEXT("LockVanguard");
	case EVeyraClientIntent::LeaveSelect:
		return TEXT("LeaveSelect");
	case EVeyraClientIntent::ChooseFluxSpell:
		return TEXT("ChooseFluxSpell");
	case EVeyraClientIntent::HoverBan:
		return TEXT("HoverBan");
	case EVeyraClientIntent::BanVanguard:
		return TEXT("BanVanguard");
	case EVeyraClientIntent::OfferTrade:
		return TEXT("OfferTrade");
	case EVeyraClientIntent::AnswerTrade:
		return TEXT("AnswerTrade");
	case EVeyraClientIntent::Reconnect:
		return TEXT("Reconnect");
	case EVeyraClientIntent::ContinueFromResults:
		return TEXT("ContinueFromResults");
	case EVeyraClientIntent::Retry:
		return TEXT("Retry");
	case EVeyraClientIntent::Quit:
		return TEXT("Quit");
	case EVeyraClientIntent::LoadHistory:
		return TEXT("LoadHistory");
	case EVeyraClientIntent::LoadMoreHistory:
		return TEXT("LoadMoreHistory");
	case EVeyraClientIntent::OpenHistoryMatch:
		return TEXT("OpenHistoryMatch");
	case EVeyraClientIntent::CloseHistoryMatch:
		return TEXT("CloseHistoryMatch");
	case EVeyraClientIntent::CreateLobby:
		return TEXT("CreateLobby");
	case EVeyraClientIntent::AcceptLobbyInvite:
		return TEXT("AcceptLobbyInvite");
	case EVeyraClientIntent::DeclineLobbyInvite:
		return TEXT("DeclineLobbyInvite");
	case EVeyraClientIntent::InviteToLobby:
		return TEXT("InviteToLobby");
	case EVeyraClientIntent::LeaveLobby:
		return TEXT("LeaveLobby");
	case EVeyraClientIntent::KickFromLobby:
		return TEXT("KickFromLobby");
	case EVeyraClientIntent::MoveInLobby:
		return TEXT("MoveInLobby");
	case EVeyraClientIntent::SetLobbyBot:
		return TEXT("SetLobbyBot");
	case EVeyraClientIntent::RemoveLobbyBot:
		return TEXT("RemoveLobbyBot");
	case EVeyraClientIntent::SetLobbySettings:
		return TEXT("SetLobbySettings");
	case EVeyraClientIntent::LaunchLobby:
		return TEXT("LaunchLobby");
	case EVeyraClientIntent::SendFriendRequest:
		return TEXT("SendFriendRequest");
	case EVeyraClientIntent::AnswerFriendRequest:
		return TEXT("AnswerFriendRequest");
	case EVeyraClientIntent::RemoveFriend:
		return TEXT("RemoveFriend");
	case EVeyraClientIntent::ResolveSettingsConflict:
		return TEXT("ResolveSettingsConflict");
	}
	return TEXT("Unknown");
}

FVeyraClientFlowConfig FVeyraClientFlowConfig::FromSettings(const UVeyraServicesSettings& Settings, const FString& BuildVersion)
{
	FVeyraClientFlowConfig Config;
	Config.BuildVersion = BuildVersion;
	Config.LaunchCodeReadTimeoutSeconds = Settings.LaunchCodeReadTimeoutSeconds;
	Config.RequestAttempts = Settings.ClientRequestAttempts;
	Config.RetryIntervalSeconds = Settings.ClientRetryIntervalSeconds;
	Config.SelectPollIntervalSeconds = Settings.SelectPollIntervalSeconds;
	Config.MatchPollIntervalSeconds = Settings.MatchPollIntervalSeconds;
	Config.MatchWaitTimeoutSeconds = Settings.MatchWaitTimeoutSeconds;
	Config.ResultPollIntervalSeconds = Settings.ResultPollIntervalSeconds;
	Config.ResultWaitTimeoutSeconds = Settings.ResultWaitTimeoutSeconds;
	Config.ReconnectPollIntervalSeconds = Settings.ReconnectPollIntervalSeconds;
	Config.PartyPollIntervalSeconds = Settings.PartyPollIntervalSeconds;
	Config.MatchFoundPollIntervalSeconds = Settings.MatchFoundPollIntervalSeconds;
	Config.LobbyPollIntervalSeconds = Settings.LobbyPollIntervalSeconds;
	Config.SocialPollIntervalSeconds = Settings.SocialPollIntervalSeconds;
	// Match data, so the client stays as long as the server does.
	Config.EndingShowSeconds = UVeyraMatchTuningSubsystem::Get().Ending.ShowSeconds;
	Config.AccountSettings.SendDelaySeconds = Settings.AccountSettingsSendDelaySeconds;
	Config.AccountSettings.RetrySeconds = Settings.AccountSettingsRetrySeconds;
	return Config;
}

FVeyraClientFlow::FVeyraClientFlow(IVeyraBackendTransport& InBackend, IVeyraClientFlowHost& InHost, FVeyraClientFlowConfig InConfig)
	: Backend(InBackend)
	, Host(InHost)
	, Config(MoveTemp(InConfig))
	, Alive(MakeShared<bool>(true))
{
}

FVeyraClientFlow::~FVeyraClientFlow() = default;

void FVeyraClientFlow::SyncAccountSettings(IVeyraAccountSettingsCache& Cache)
{
	FVeyraAccountSettingsSync::FCallbacks Callbacks;
	Callbacks.OnReady = [this] { Resume(); };
	Callbacks.OnConflictChanged = [this] {
		Snapshot.bSettingsConflict = AccountSettings->HasConflict();
		Broadcast();
	};
	Callbacks.OnSessionRefused = [this] { EndSession(); };
	AccountSettings = MakeUnique<FVeyraAccountSettingsSync>(Backend, Cache, Config.AccountSettings, MoveTemp(Callbacks));
}

void FVeyraClientFlow::Start(const FString& ConfigurationProblem)
{
	if (!ConfigurationProblem.IsEmpty())
	{
		FailSignIn(EFailure::Misconfigured, ConfigurationProblem);
		return;
	}
	bReadingLaunchCode = true;
	LaunchCodeDeadline = Host.Now() + Config.LaunchCodeReadTimeoutSeconds;
	Log(TEXT("waiting for the launch code on standard input."));
	// A launcher requests the code only now, so the code's short life starts once the game can read it.
	Host.WriteHandshake(VeyraLaunchHandshake::AwaitingLaunchCode);
	Broadcast();
}

void FVeyraClientFlow::Tick()
{
	if (bReadingLaunchCode)
	{
		TickLaunchCode();
	}

	const double Now = Host.Now();
	if (AccountSettings)
	{
		AccountSettings->Tick(Now);
	}
	TArray<TFunction<void()>> Due;
	TArray<FWait> Pending = MoveTemp(Waits);
	Waits.Reset();
	for (FWait& Wait : Pending)
	{
		if (Wait.Epoch != Epoch)
		{
			continue;
		}
		if (Wait.At <= Now)
		{
			Due.Add(MoveTemp(Wait.Run));
		}
		else
		{
			Waits.Add(MoveTemp(Wait));
		}
	}
	const uint32 TickEpoch = Epoch;
	for (TFunction<void()>& Run : Due)
	{
		// An earlier wait may have changed the state, which cancels the rest.
		if (Epoch != TickEpoch)
		{
			break;
		}
		Run();
	}
}

bool FVeyraClientFlow::IsIntentAllowed(EVeyraClientState State, EVeyraClientIntent Intent)
{
	switch (Intent)
	{
	case EVeyraClientIntent::ChooseStarter:
		return State == EVeyraClientState::StarterChoice;
	case EVeyraClientIntent::StartPractice:
	case EVeyraClientIntent::SelectMode:
	case EVeyraClientIntent::SetReady:
	case EVeyraClientIntent::FindMatch:
	case EVeyraClientIntent::CancelQueue:
		return State == EVeyraClientState::Shell;
	case EVeyraClientIntent::AcceptMatch:
	case EVeyraClientIntent::DeclineMatch:
		return State == EVeyraClientState::MatchFound;
	case EVeyraClientIntent::HoverVanguard:
	case EVeyraClientIntent::LockVanguard:
	case EVeyraClientIntent::LeaveSelect:
	case EVeyraClientIntent::ChooseFluxSpell:
	case EVeyraClientIntent::HoverBan:
	case EVeyraClientIntent::BanVanguard:
	case EVeyraClientIntent::OfferTrade:
	case EVeyraClientIntent::AnswerTrade:
		return State == EVeyraClientState::Selecting;
	case EVeyraClientIntent::Reconnect:
		return State == EVeyraClientState::ReconnectOnly;
	case EVeyraClientIntent::ContinueFromResults:
		return State == EVeyraClientState::Results;
	case EVeyraClientIntent::Retry:
	case EVeyraClientIntent::Quit:
	case EVeyraClientIntent::ResolveSettingsConflict:
		return true;
	// The ordinary client only: not through Match Found, a committed select or Reconnect-only (UX-51).
	case EVeyraClientIntent::LoadHistory:
	case EVeyraClientIntent::LoadMoreHistory:
	case EVeyraClientIntent::OpenHistoryMatch:
	case EVeyraClientIntent::CloseHistoryMatch:
		return State == EVeyraClientState::Shell;
	// A player joins one lobby at a time, from the shell (ADR-021).
	case EVeyraClientIntent::CreateLobby:
	case EVeyraClientIntent::AcceptLobbyInvite:
		return State == EVeyraClientState::Shell;
	case EVeyraClientIntent::InviteToLobby:
	case EVeyraClientIntent::LeaveLobby:
	case EVeyraClientIntent::KickFromLobby:
	case EVeyraClientIntent::MoveInLobby:
	case EVeyraClientIntent::SetLobbyBot:
	case EVeyraClientIntent::RemoveLobbyBot:
	case EVeyraClientIntent::SetLobbySettings:
	case EVeyraClientIntent::LaunchLobby:
		return State == EVeyraClientState::Lobby;
	// The friends panel shows in the shell and the lobby alike (Art Bible §7).
	case EVeyraClientIntent::DeclineLobbyInvite:
	case EVeyraClientIntent::SendFriendRequest:
	case EVeyraClientIntent::AnswerFriendRequest:
	case EVeyraClientIntent::RemoveFriend:
		return State == EVeyraClientState::Shell || State == EVeyraClientState::Lobby;
	}
	return false;
}

bool FVeyraClientFlow::CanIssue(EVeyraClientIntent Intent) const
{
	if (Intent == EVeyraClientIntent::Quit)
	{
		return true;
	}
	if (Intent == EVeyraClientIntent::Retry)
	{
		return !Snapshot.bBusy && Snapshot.Problem.IsSet() && Snapshot.Problem->bCanRetry;
	}
	if (Intent == EVeyraClientIntent::ResolveSettingsConflict)
	{
		// Settings are not the flow's step: the choice shows over whatever the player is doing.
		return Snapshot.bSettingsConflict;
	}
	if (Snapshot.bBusy || !IsIntentAllowed(Snapshot.State, Intent))
	{
		return false;
	}
	// A backend with custom lobbies switched off offers none to open or join (ADR-021 §1).
	if ((Intent == EVeyraClientIntent::CreateLobby || Intent == EVeyraClientIntent::AcceptLobbyInvite) && !Snapshot.bCustomLobbies)
	{
		return false;
	}
	const TOptional<VeyraBackendProtocol::FParty>& Party = Snapshot.Party;
	switch (Intent)
	{
	case EVeyraClientIntent::StartPractice:
		// A party in matchmaking cannot practise as well.
		return !Party.IsSet() || Party->Status == EPartyStatus::Idle;
	case EVeyraClientIntent::SelectMode:
		// Choosing a mode without a party makes one the player leads (UX-6).
		return !Snapshot.Modes.IsEmpty() && (!Party.IsSet() || LeadsIdleParty());
	case EVeyraClientIntent::SetReady:
		return Party.IsSet() && Party->Status == EPartyStatus::Idle && !Party->Mode.IsEmpty();
	case EVeyraClientIntent::FindMatch:
	{
		const VeyraBackendProtocol::FModeInfo* Mode = Party.IsSet() ? FindMode(Party->Mode) : nullptr;
		return LeadsIdleParty() && Mode && Mode->bEnabled && Mode->bMatchmade && Party->Members.Num() <= Mode->HumanPlayersPerTeam && Party->AllReady();
	}
	case EVeyraClientIntent::CancelQueue:
		return Party.IsSet() && Party->Status == EPartyStatus::Queued && Leads(Snapshot);
	case EVeyraClientIntent::AcceptMatch:
	case EVeyraClientIntent::DeclineMatch:
		return Snapshot.MatchFound.State == PendingAnswer && Snapshot.MatchFound.You == PendingAnswer;
	case EVeyraClientIntent::HoverVanguard:
	{
		// A lock is permanent (Battleground Bible §15). In a draft a pick may be hovered before its turn,
		// as the player's intent (ADR-042 §1).
		const VeyraBackendProtocol::FSelectSeat* You = Snapshot.Select.FindYou();
		return Snapshot.Select.State == ESelectState::Picking && Snapshot.Select.Phase != VeyraBackendProtocol::ESelectPhase::Final && You
			&& You->Locked.IsEmpty() && !Snapshot.AvailableVanguards.IsEmpty();
	}
	case EVeyraClientIntent::LockVanguard:
		// In a draft only in the player's pick turn.
		return Snapshot.Select.YouMayLock() && !Snapshot.AvailableVanguards.IsEmpty();
	case EVeyraClientIntent::HoverBan:
	case EVeyraClientIntent::BanVanguard:
		return Snapshot.Select.YouBan() && !Snapshot.ReleasedVanguards.IsEmpty();
	case EVeyraClientIntent::OfferTrade:
	{
		const VeyraBackendProtocol::FSelectSeat* You = LockedTrader(Snapshot.Select);
		return You && Snapshot.Select.Seats.ContainsByPredicate([You](const VeyraBackendProtocol::FSelectSeat& Seat) {
			return !Seat.bYou && Seat.Side == You->Side && !Seat.Locked.IsEmpty();
		});
	}
	case EVeyraClientIntent::AnswerTrade:
		return LockedTrader(Snapshot.Select)
			&& Snapshot.Select.Seats.ContainsByPredicate([](const VeyraBackendProtocol::FSelectSeat& Seat) { return Seat.bOffersYou; });
	case EVeyraClientIntent::LeaveSelect:
		// Practice has no one to dodge; only its timer ends it (ADR-010). A custom select returns to its lobby.
		return IsLeavable(Snapshot.Select) && Snapshot.Select.State == ESelectState::Picking;
	case EVeyraClientIntent::ChooseFluxSpell:
		// Spells stay free to change after lock-in, until the match starts (Pre-Game Client UX Bible 36).
		return Snapshot.Select.State == ESelectState::Picking && Snapshot.Select.FindYou() != nullptr;
	case EVeyraClientIntent::LoadMoreHistory:
		return Snapshot.History.bLoaded && !Snapshot.History.Next.IsEmpty() && !Snapshot.History.Opened.IsSet();
	case EVeyraClientIntent::OpenHistoryMatch:
		return Snapshot.History.bLoaded && !Snapshot.History.Opened.IsSet();
	case EVeyraClientIntent::CloseHistoryMatch:
		return Snapshot.History.Opened.IsSet();
	case EVeyraClientIntent::CreateLobby:
		// A party in matchmaking cannot also host a custom match.
		return !Party.IsSet() || Party->Status == EPartyStatus::Idle;
	case EVeyraClientIntent::AcceptLobbyInvite:
		return !Snapshot.Social.LobbyInvites.IsEmpty() && (!Party.IsSet() || Party->Status == EPartyStatus::Idle);
	case EVeyraClientIntent::DeclineLobbyInvite:
		return !Snapshot.Social.LobbyInvites.IsEmpty();
	case EVeyraClientIntent::InviteToLobby:
		return HostsOpenLobby() && !Snapshot.Social.Friends.Friends.IsEmpty();
	case EVeyraClientIntent::KickFromLobby:
	case EVeyraClientIntent::MoveInLobby:
	case EVeyraClientIntent::SetLobbyBot:
	case EVeyraClientIntent::RemoveLobbyBot:
	case EVeyraClientIntent::SetLobbySettings:
	case EVeyraClientIntent::LaunchLobby:
		// The host decides everything about the lobby (Custom Matches Bible §1), until it starts.
		return HostsOpenLobby();
	case EVeyraClientIntent::LeaveLobby:
		return Snapshot.Lobby.IsSet() && !Snapshot.Lobby->bSelecting;
	case EVeyraClientIntent::AnswerFriendRequest:
		return !Snapshot.Social.Friends.Incoming.IsEmpty();
	case EVeyraClientIntent::RemoveFriend:
		return !Snapshot.Social.Friends.Friends.IsEmpty();
	default:
		return true;
	}
}

double FVeyraClientFlow::GetRemainingPickSeconds() const
{
	return Snapshot.State == EVeyraClientState::Selecting ? FMath::Max(0.0, Snapshot.PickEndsAt - Host.Now()) : 0.0;
}

double FVeyraClientFlow::GetQueuedSeconds() const
{
	const bool bInShell = Snapshot.State == EVeyraClientState::Shell || Snapshot.State == EVeyraClientState::MatchFound;
	const bool bQueued = Snapshot.Party.IsSet() && Snapshot.Party->Status != EPartyStatus::Idle;
	return bInShell && bQueued ? FMath::Max(0.0, Host.Now() - Snapshot.QueuedSince) : 0.0;
}

double FVeyraClientFlow::GetRemainingAcceptSeconds() const
{
	return Snapshot.State == EVeyraClientState::MatchFound ? FMath::Max(0.0, Snapshot.AcceptEndsAt - Host.Now()) : 0.0;
}

// Signing in ------------------------------------------------------------------------------------

void FVeyraClientFlow::TickLaunchCode()
{
	FString Line;
	const EVeyraPipeRead Read = Host.PollLaunchCode(Line);
	if (Read == EVeyraPipeRead::Pending)
	{
		if (Host.Now() >= LaunchCodeDeadline)
		{
			FailSignIn(EFailure::NoLaunchCode, TEXT("no launch code arrived on standard input within LaunchCodeReadTimeoutSeconds"));
		}
		return;
	}

	bReadingLaunchCode = false;
	switch (Read)
	{
	case EVeyraPipeRead::Line:
		if (VeyraBackendProtocol::IsLaunchCode(Line))
		{
			Redeem(Line);
		}
		else
		{
			FailSignIn(EFailure::InvalidLaunchCode, TEXT("standard input did not hold a launch code"));
		}
		break;
	case EVeyraPipeRead::EndOfInput:
		FailSignIn(EFailure::NoLaunchCode, TEXT("standard input closed without a launch code"));
		break;
	case EVeyraPipeRead::TooLong:
		FailSignIn(EFailure::InvalidLaunchCode, TEXT("standard input held a line far too long to be a launch code"));
		break;
	case EVeyraPipeRead::NotAPipe:
		FailSignIn(EFailure::NoLaunchCode, TEXT("standard input is not a pipe; whoever starts the game must pass the launch code through one"));
		break;
	default:
		FailSignIn(EFailure::NoLaunchCode, TEXT("reading standard input failed"));
		break;
	}
}

void FVeyraClientFlow::Redeem(const FString& LaunchCode)
{
	Log(TEXT("redeeming the launch code."));
	// A launch code is single use, so a failed redemption is never tried again.
	Backend.Post(RedeemPath, FString(), VeyraBackendProtocol::BuildRedeemBody(LaunchCode, Config.BuildVersion),
		[this, WeakAlive = TWeakPtr<bool>(Alive)](const FVeyraBackendResponse& Response) {
			if (WeakAlive.IsValid())
			{
				OnRedeemed(Response);
			}
		});
}

void FVeyraClientFlow::OnRedeemed(const FVeyraBackendResponse& Response)
{
	if (!Response.IsSuccess())
	{
		FailSignIn(Response.IsTransient() ? EFailure::BackendUnreachable : EFailure::SignInRefused,
			TEXT("the backend did not redeem the launch code: ") + Response.Describe());
		return;
	}
	VeyraBackendProtocol::FGameSession Session;
	FString Problem;
	if (!VeyraBackendProtocol::ParseGameSession(Response.Body, Session, Problem))
	{
		FailSignIn(EFailure::BadAnswer, TEXT("the backend's answer to the launch code was not understood: ") + Problem);
		return;
	}
	GameSession = MoveTemp(Session.Token);
	Snapshot.DisplayName = Session.DisplayName;
	Snapshot.AccountId = Session.AccountId;
	Host.WriteHandshake(VeyraLaunchHandshake::SignedIn);
	Log(FString::Printf(TEXT("signed in as %s."), *Session.DisplayName));
	if (!AccountSettings)
	{
		Resume();
		return;
	}
	// The player's settings, and their choice if they changed elsewhere too, come before the shell (ADR-024 §1).
	Enter(EVeyraClientState::Loading);
	Broadcast();
	AccountSettings->SignIn(Snapshot.AccountId, GameSession, Host.Now());
}

void FVeyraClientFlow::FailSignIn(EFailure Failure, const FString& Reason)
{
	bReadingLaunchCode = false;
	// The launcher waits for this line, then offers its own Retry with a new code.
	Host.WriteHandshake(VeyraLaunchHandshake::FailedLine(Failure));
	Enter(EVeyraClientState::SignInFailed);
	Snapshot.SignInFailure = Failure;
	Snapshot.Problem = FVeyraClientProblem{ VeyraLaunchHandshake::FailureCode(Failure), Reason, /*bCanRetry*/ false };
	UE_LOG(LogVeyraServices, Error, TEXT("VeyraClientFlow: signing in failed: %s."), *VeyraBackendProtocol::RedactCredentials(Reason));
	Broadcast();
}

// Where the player is ---------------------------------------------------------------------------

void FVeyraClientFlow::Resume(const FString& Notice)
{
	// A live match outranks everything, then a select in progress, then the starter choice (ADR-010 §2).
	// A match found shows through the party, which the shell reads.
	Enter(EVeyraClientState::Loading, Notice);
	Broadcast();
	Call(EVerb::Get, MyMatchPath, FString(), [this](const FVeyraBackendResponse& Response) {
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("the player's match"), [this] { Resume(); });
			return;
		}
		VeyraBackendProtocol::FMyMatch Match;
		FString Problem;
		if (!VeyraBackendProtocol::ParseMyMatch(Response.Body, Match, Problem))
		{
			ShowBadAnswer(TEXT("the player's match"), Problem, [this] { Resume(); });
			return;
		}
		if (Match.bHasMatch)
		{
			EnterReconnectOnly(Match.MatchId);
			return;
		}
		Call(EVerb::Get, MySelectPath, FString(), [this](const FVeyraBackendResponse& SelectResponse) {
			if (!SelectResponse.IsSuccess())
			{
				ShowRefusal(SelectResponse, TEXT("the player's champion select"), [this] { Resume(); });
				return;
			}
			TOptional<VeyraBackendProtocol::FSelect> Select;
			FString SelectProblem;
			if (!VeyraBackendProtocol::ParseSelect(SelectResponse.Body, Select, SelectProblem))
			{
				ShowBadAnswer(TEXT("the player's champion select"), SelectProblem, [this] { Resume(); });
				return;
			}
			if (Select.IsSet())
			{
				EnterSelecting(*Select);
				return;
			}
			LoadProfile();
		});
	});
}

void FVeyraClientFlow::LoadProfile()
{
	Call(EVerb::Get, ProfilePath, FString(), [this](const FVeyraBackendResponse& Response) {
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("the player's profile"), [this] { Resume(); });
			return;
		}
		VeyraBackendProtocol::FProfile Profile;
		FString Problem;
		if (!VeyraBackendProtocol::ParseProfile(Response.Body, Profile, Problem))
		{
			ShowBadAnswer(TEXT("the player's profile"), Problem, [this] { Resume(); });
			return;
		}
		if (Profile.bTutorialCompleted)
		{
			LoadLobby(Snapshot.Notice);
			return;
		}
		Call(EVerb::Get, VanguardsPath, FString(), [this](const FVeyraBackendResponse& VanguardsResponse) {
			if (!VanguardsResponse.IsSuccess())
			{
				ShowRefusal(VanguardsResponse, TEXT("the starters"), [this] { Resume(); });
				return;
			}
			VeyraBackendProtocol::FVanguardAccess Access;
			FString AccessProblem;
			if (!VeyraBackendProtocol::ParseVanguardAccess(VanguardsResponse.Body, Access, AccessProblem))
			{
				ShowBadAnswer(TEXT("the starters"), AccessProblem, [this] { Resume(); });
				return;
			}
			Enter(EVeyraClientState::StarterChoice);
			Snapshot.Starters = MoveTemp(Access.Starters);
			Broadcast();
		});
	});
}

bool FVeyraClientFlow::ChooseStarter(const FString& VanguardId)
{
	if (!CanIssue(EVeyraClientIntent::ChooseStarter) || !Snapshot.Starters.Contains(VanguardId))
	{
		return false;
	}
	Log(FString::Printf(TEXT("choosing %s as the starter."), *VanguardId));
	SetBusy(true);
	Call(EVerb::Post, StarterPath, VeyraBackendProtocol::BuildVanguardBody(VanguardId), [this](const FVeyraBackendResponse& Response) {
		SetBusy(false);
		if (Response.IsSuccess())
		{
			EnterShell(FString());
		}
		else if (IsRefusal(Response, TEXT("already_completed")))
		{
			// An earlier attempt went through after all.
			Resume();
		}
		else
		{
			ShowRefusal(Response, TEXT("the starter"), nullptr);
		}
	});
	return true;
}

bool FVeyraClientFlow::StartPractice()
{
	if (!CanIssue(EVeyraClientIntent::StartPractice))
	{
		return false;
	}
	Log(TEXT("starting practice."));
	SetBusy(true);
	Call(EVerb::Post, PracticePath, FString(), [this](const FVeyraBackendResponse& Response) {
		SetBusy(false);
		if (IsRefusal(Response, TEXT("tutorial_required")) || IsRefusal(Response, TEXT("busy")))
		{
			// The player is somewhere else than this screen knew: find out where.
			Resume();
			return;
		}
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("practice"), nullptr);
			return;
		}
		TOptional<VeyraBackendProtocol::FSelect> Select;
		FString Problem;
		if (!VeyraBackendProtocol::ParseSelect(Response.Body, Select, Problem) || !Select.IsSet())
		{
			ShowBadAnswer(TEXT("the practice select"), Problem.IsEmpty() ? FString(TEXT("the select is null")) : Problem, [this] { Resume(); });
			return;
		}
		EnterSelecting(*Select);
	});
	return true;
}

// The shell, the party and its queue ------------------------------------------------------------

void FVeyraClientFlow::EnterShell(const FString& Notice)
{
	Enter(EVeyraClientState::Shell, Notice);
	bExplainQueue = Notice == MissedNotice || Notice == AbandonedNotice;
	Snapshot.Lobby.Reset();
	Broadcast();
	if (Snapshot.Modes.IsEmpty())
	{
		LoadModes();
	}
	// There is no push channel yet: a queue's progress, and a match found, arrive through the party (ADR-010 §10).
	PollParty();
	PollSocial();
}

void FVeyraClientFlow::LoadModes()
{
	Call(EVerb::Get, ModesPath, FString(), [this](const FVeyraBackendResponse& Response) {
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("the modes"), [this] { LoadModes(); });
			return;
		}
		TArray<VeyraBackendProtocol::FModeInfo> Modes;
		FString Problem;
		if (!VeyraBackendProtocol::ParseModes(Response.Body, Modes, Problem))
		{
			ShowBadAnswer(TEXT("the modes"), Problem, [this] { LoadModes(); });
			return;
		}
		Snapshot.Modes = MoveTemp(Modes);
		Broadcast();
	});
}

void FVeyraClientFlow::PollParty()
{
	const uint32 Sequence = ++PartySequence;
	Call(EVerb::Get, PartyPath, FString(), [this, Sequence](const FVeyraBackendResponse& Response) {
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("the player's party"), [this] { PollParty(); });
			return;
		}
		TOptional<VeyraBackendProtocol::FParty> Party;
		FString Problem;
		if (!VeyraBackendProtocol::ParseParty(Response.Body, Party, Problem))
		{
			ShowBadAnswer(TEXT("the player's party"), Problem, [this] { PollParty(); });
			return;
		}
		if (!ApplyParty(Sequence, MoveTemp(Party)))
		{
			After(Config.PartyPollIntervalSeconds, [this] { PollParty(); });
			return;
		}
		if (bExplainQueue)
		{
			bExplainQueue = false;
			if (Snapshot.Party.IsSet() && Snapshot.Party->Status != EPartyStatus::Idle)
			{
				// Not at fault: matchmaking put the party back in the queue, keeping its place (§3).
				Snapshot.Notice = RequeuedNotice;
				Broadcast();
			}
		}
		if (Snapshot.Party.IsSet() && (Snapshot.Party->Status == EPartyStatus::Found || Snapshot.Party->Status == EPartyStatus::Selecting))
		{
			FollowParty();
			return;
		}
		After(Config.PartyPollIntervalSeconds, [this] { PollParty(); });
	});
}

void FVeyraClientFlow::CallParty(EVerb Verb, const TCHAR* Path, const FString& Body, const TCHAR* What)
{
	const uint32 Sequence = ++PartySequence;
	SetBusy(true);
	Call(Verb, Path, Body, [this, Sequence, What](const FVeyraBackendResponse& Response) {
		SetBusy(false);
		if (IsRefusal(Response, TEXT("party_locked")))
		{
			// Matchmaking moved the party on first; the next read shows where.
			return;
		}
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, What, nullptr);
			return;
		}
		TOptional<VeyraBackendProtocol::FParty> Party;
		FString Problem;
		if (!VeyraBackendProtocol::ParseParty(Response.Body, Party, Problem))
		{
			ShowBadAnswer(What, Problem, nullptr);
			return;
		}
		ApplyParty(Sequence, MoveTemp(Party));
	});
}

bool FVeyraClientFlow::ApplyParty(uint32 Sequence, TOptional<VeyraBackendProtocol::FParty> Party)
{
	if (Sequence < ShownPartySequence)
	{
		return false;
	}
	ShownPartySequence = Sequence;
	if (Party.IsSet() && Party->Status != EPartyStatus::Idle)
	{
		Snapshot.QueuedSince = Host.Now() - Party->QueuedSeconds;
	}
	Snapshot.Party = MoveTemp(Party);
	Broadcast();
	return true;
}

void FVeyraClientFlow::FollowParty()
{
	if (Snapshot.Party->Status == EPartyStatus::Found)
	{
		Call(EVerb::Get, MatchFoundPath, FString(), [this](const FVeyraBackendResponse& Response) {
			if (!Response.IsSuccess())
			{
				ShowRefusal(Response, TEXT("the match found"), [this] { PollParty(); });
				return;
			}
			TOptional<VeyraBackendProtocol::FMatchFound> Found;
			FString Problem;
			if (!VeyraBackendProtocol::ParseMatchFound(Response.Body, Found, Problem))
			{
				ShowBadAnswer(TEXT("the match found"), Problem, [this] { PollParty(); });
				return;
			}
			if (Found.IsSet())
			{
				EnterMatchFound(*Found);
				return;
			}
			// It ended before this read; the party shows what came of it.
			After(Config.PartyPollIntervalSeconds, [this] { PollParty(); });
		});
		return;
	}
	Call(EVerb::Get, MySelectPath, FString(), [this](const FVeyraBackendResponse& Response) {
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("the player's champion select"), [this] { PollParty(); });
			return;
		}
		TOptional<VeyraBackendProtocol::FSelect> Select;
		FString Problem;
		if (!VeyraBackendProtocol::ParseSelect(Response.Body, Select, Problem))
		{
			ShowBadAnswer(TEXT("the player's champion select"), Problem, [this] { PollParty(); });
			return;
		}
		if (Select.IsSet())
		{
			EnterSelecting(*Select);
			return;
		}
		After(Config.PartyPollIntervalSeconds, [this] { PollParty(); });
	});
}

const VeyraBackendProtocol::FModeInfo* FVeyraClientFlow::FindMode(const FString& ModeId) const
{
	return Snapshot.Modes.FindByPredicate([&ModeId](const VeyraBackendProtocol::FModeInfo& Mode) { return Mode.Id == ModeId; });
}

bool FVeyraClientFlow::LeadsIdleParty() const
{
	return Snapshot.Party.IsSet() && Snapshot.Party->Status == EPartyStatus::Idle && Leads(Snapshot);
}

bool FVeyraClientFlow::SelectMode(const FString& ModeId)
{
	const VeyraBackendProtocol::FModeInfo* Mode = FindMode(ModeId);
	if (!CanIssue(EVeyraClientIntent::SelectMode) || !Mode || !Mode->bEnabled || !Mode->bMatchmade)
	{
		return false;
	}
	Log(FString::Printf(TEXT("choosing the mode %s."), *ModeId));
	CallParty(EVerb::Put, PartyModePath, VeyraBackendProtocol::BuildModeBody(ModeId), TEXT("the mode"));
	return true;
}

bool FVeyraClientFlow::SetReady(bool bReady)
{
	if (!CanIssue(EVeyraClientIntent::SetReady))
	{
		return false;
	}
	Log(bReady ? TEXT("ready.") : TEXT("not ready."));
	CallParty(EVerb::Put, PartyReadyPath, VeyraBackendProtocol::BuildReadyBody(bReady), TEXT("Ready"));
	return true;
}

bool FVeyraClientFlow::FindMatch()
{
	if (!CanIssue(EVeyraClientIntent::FindMatch))
	{
		return false;
	}
	Log(FString::Printf(TEXT("queueing for %s."), *Snapshot.Party->Mode));
	CallParty(EVerb::Post, QueuePath, FString(), TEXT("the queue"));
	return true;
}

bool FVeyraClientFlow::CancelQueue()
{
	if (!CanIssue(EVeyraClientIntent::CancelQueue))
	{
		return false;
	}
	Log(TEXT("leaving the queue."));
	CallParty(EVerb::Delete, QueuePath, FString(), TEXT("leaving the queue"));
	return true;
}

// The custom lobby -------------------------------------------------------------------------------

void FVeyraClientFlow::LoadLobby(const FString& Notice)
{
	Call(EVerb::Get, LobbyPath, FString(), [this, Notice](const FVeyraBackendResponse& Response) {
		// A backend with custom lobbies switched off serves no lobby routes; "no lobby" is a 200.
		Snapshot.bCustomLobbies = Response.Status != NotFoundStatus;
		if (!Snapshot.bCustomLobbies)
		{
			EnterShell(Notice);
			return;
		}
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("the player's lobby"), [this] { Resume(); });
			return;
		}
		TOptional<VeyraBackendProtocol::FLobby> Lobby;
		FString Problem;
		if (!VeyraBackendProtocol::ParseLobby(Response.Body, Lobby, Problem))
		{
			ShowBadAnswer(TEXT("the player's lobby"), Problem, [this] { Resume(); });
			return;
		}
		if (Lobby.IsSet())
		{
			EnterLobby(MoveTemp(*Lobby), Notice);
			return;
		}
		EnterShell(Notice);
	});
}

void FVeyraClientFlow::EnterLobby(VeyraBackendProtocol::FLobby Lobby, const FString& Notice)
{
	Enter(EVeyraClientState::Lobby, Notice);
	Log(FString::Printf(TEXT("in custom lobby %s%s."), *Lobby.Id, Lobby.HostAccountId == Snapshot.AccountId ? TEXT(", as its host") : TEXT("")));
	ApplyLobby(++LobbySequence, MoveTemp(Lobby));
	PollSocial();
	if (Snapshot.Lobby->bSelecting)
	{
		FollowLobby();
		return;
	}
	// There is no push channel yet: the lobby's changes, and its start, arrive by reading it.
	After(Config.LobbyPollIntervalSeconds, [this] { PollLobby(); });
}

void FVeyraClientFlow::PollLobby()
{
	const uint32 Sequence = ++LobbySequence;
	Call(EVerb::Get, LobbyPath, FString(), [this, Sequence](const FVeyraBackendResponse& Response) {
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("the player's lobby"), [this] { PollLobby(); });
			return;
		}
		TOptional<VeyraBackendProtocol::FLobby> Lobby;
		FString Problem;
		if (!VeyraBackendProtocol::ParseLobby(Response.Body, Lobby, Problem))
		{
			ShowBadAnswer(TEXT("the player's lobby"), Problem, [this] { PollLobby(); });
			return;
		}
		if (!Lobby.IsSet())
		{
			// The host removed the player, or the lobby closed.
			Log(TEXT("the player is no longer in a lobby."));
			EnterShell(LobbyGoneNotice);
			return;
		}
		if (ApplyLobby(Sequence, MoveTemp(Lobby)) && Snapshot.Lobby->bSelecting)
		{
			FollowLobby();
			return;
		}
		After(Config.LobbyPollIntervalSeconds, [this] { PollLobby(); });
	});
}

void FVeyraClientFlow::CallLobby(EVerb Verb, const FString& Path, const FString& Body, const TCHAR* What)
{
	const uint32 Sequence = ++LobbySequence;
	SetBusy(true);
	Call(Verb, Path, Body, [this, Sequence, What](const FVeyraBackendResponse& Response) {
		SetBusy(false);
		if (IsRefusal(Response, TEXT("lobby_locked")))
		{
			// Its champion select started first; the next read shows it.
			return;
		}
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, What, nullptr);
			return;
		}
		TOptional<VeyraBackendProtocol::FLobby> Lobby;
		FString Problem;
		if (!VeyraBackendProtocol::ParseLobby(Response.Body, Lobby, Problem) || !Lobby.IsSet())
		{
			ShowBadAnswer(What, Problem.IsEmpty() ? FString(TEXT("the lobby is null")) : Problem, nullptr);
			return;
		}
		if (ApplyLobby(Sequence, MoveTemp(Lobby)) && Snapshot.Lobby->bSelecting)
		{
			// The host's start: its select is open before the backend answers.
			FollowLobby();
		}
	});
}

bool FVeyraClientFlow::ApplyLobby(uint32 Sequence, TOptional<VeyraBackendProtocol::FLobby> Lobby)
{
	if (Sequence < ShownLobbySequence)
	{
		return false;
	}
	ShownLobbySequence = Sequence;
	Snapshot.Lobby = MoveTemp(Lobby);
	Broadcast();
	return true;
}

void FVeyraClientFlow::FollowLobby()
{
	Call(EVerb::Get, MySelectPath, FString(), [this](const FVeyraBackendResponse& Response) {
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("the lobby's champion select"), [this] { PollLobby(); });
			return;
		}
		TOptional<VeyraBackendProtocol::FSelect> Select;
		FString Problem;
		if (!VeyraBackendProtocol::ParseSelect(Response.Body, Select, Problem))
		{
			ShowBadAnswer(TEXT("the lobby's champion select"), Problem, [this] { PollLobby(); });
			return;
		}
		if (Select.IsSet())
		{
			EnterSelecting(*Select);
			return;
		}
		// It ended before this read; the lobby shows what came of it.
		After(Config.LobbyPollIntervalSeconds, [this] { PollLobby(); });
	});
}

bool FVeyraClientFlow::HostsOpenLobby() const
{
	return Snapshot.Lobby.IsSet() && !Snapshot.Lobby->bSelecting && Snapshot.Lobby->HostAccountId == Snapshot.AccountId;
}

const VeyraBackendProtocol::FLobbySeat* FVeyraClientFlow::FindLobbySeat(const FString& Side, int32 Index) const
{
	return Snapshot.Lobby.IsSet()
		? Snapshot.Lobby->Seats.FindByPredicate([&Side, Index](const VeyraBackendProtocol::FLobbySeat& Seat) { return Seat.Side == Side && Seat.Index == Index; })
		: nullptr;
}

bool FVeyraClientFlow::CreateLobby()
{
	if (!CanIssue(EVeyraClientIntent::CreateLobby))
	{
		return false;
	}
	Log(TEXT("opening a custom lobby."));
	SetBusy(true);
	Call(EVerb::Post, LobbyPath, FString(), [this](const FVeyraBackendResponse& Response) {
		SetBusy(false);
		if (IsRefusal(Response, TEXT("already_in_lobby")))
		{
			// The player is in a lobby this screen did not know of: find out where.
			Resume();
			return;
		}
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("the custom lobby"), nullptr);
			return;
		}
		TOptional<VeyraBackendProtocol::FLobby> Lobby;
		FString Problem;
		if (!VeyraBackendProtocol::ParseLobby(Response.Body, Lobby, Problem) || !Lobby.IsSet())
		{
			ShowBadAnswer(TEXT("the custom lobby"), Problem.IsEmpty() ? FString(TEXT("the lobby is null")) : Problem, [this] { Resume(); });
			return;
		}
		EnterLobby(MoveTemp(*Lobby), FString());
	});
	return true;
}

bool FVeyraClientFlow::AcceptLobbyInvite(const FString& InviteId)
{
	const VeyraBackendProtocol::FLobbyInvite* Invite =
		Snapshot.Social.LobbyInvites.FindByPredicate([&InviteId](const VeyraBackendProtocol::FLobbyInvite& Candidate) { return Candidate.Id == InviteId; });
	if (!CanIssue(EVeyraClientIntent::AcceptLobbyInvite) || !Invite)
	{
		return false;
	}
	const FString Inviter = Invite->Inviter.DisplayName;
	Log(FString::Printf(TEXT("joining %s's lobby."), *Inviter));
	SetBusy(true);
	Call(EVerb::Post, LobbyInvitePath(InviteId, TEXT("accept")), FString(), [this, Inviter](const FVeyraBackendResponse& Response) {
		SetBusy(false);
		if (IsRefusal(Response, TEXT("already_in_lobby")))
		{
			Resume();
			return;
		}
		if (!Response.IsSuccess())
		{
			// An invitation that expired, or a lobby that filled or started: the panel says so.
			ShowSocialFeedback(RefusalCode(Response), Inviter);
			ReadSocial(/*bThenPoll*/ false);
			return;
		}
		TOptional<VeyraBackendProtocol::FLobby> Lobby;
		FString Problem;
		if (!VeyraBackendProtocol::ParseLobby(Response.Body, Lobby, Problem) || !Lobby.IsSet())
		{
			ShowBadAnswer(TEXT("the invitation's lobby"), Problem.IsEmpty() ? FString(TEXT("the lobby is null")) : Problem, [this] { Resume(); });
			return;
		}
		EnterLobby(MoveTemp(*Lobby), FString());
	});
	return true;
}

bool FVeyraClientFlow::DeclineLobbyInvite(const FString& InviteId)
{
	const VeyraBackendProtocol::FLobbyInvite* Invite =
		Snapshot.Social.LobbyInvites.FindByPredicate([&InviteId](const VeyraBackendProtocol::FLobbyInvite& Candidate) { return Candidate.Id == InviteId; });
	if (!CanIssue(EVeyraClientIntent::DeclineLobbyInvite) || !Invite)
	{
		return false;
	}
	Log(FString::Printf(TEXT("declining %s's invitation."), *Invite->Inviter.DisplayName));
	CallSocial(EVerb::Post, LobbyInvitePath(InviteId, TEXT("decline")), FString(), Invite->Inviter.DisplayName, nullptr);
	return true;
}

bool FVeyraClientFlow::InviteToLobby(const FString& AccountId)
{
	const VeyraBackendProtocol::FAccount* Friend = Snapshot.Social.Friends.Friends.FindByPredicate(
		[&AccountId](const VeyraBackendProtocol::FAccount& Candidate) { return Candidate.Id == AccountId; });
	if (!CanIssue(EVeyraClientIntent::InviteToLobby) || !Friend || Snapshot.Lobby->FindMember(AccountId))
	{
		return false;
	}
	Log(FString::Printf(TEXT("inviting %s into the lobby."), *Friend->DisplayName));
	CallSocial(EVerb::Post, LobbyInvitesPath, VeyraBackendProtocol::BuildAccountBody(AccountId), Friend->DisplayName,
		[this, Name = Friend->DisplayName](const FVeyraBackendResponse&) { ShowSocialFeedback(LobbyInvitedFeedback, Name); });
	return true;
}

bool FVeyraClientFlow::LeaveLobby()
{
	if (!CanIssue(EVeyraClientIntent::LeaveLobby))
	{
		return false;
	}
	Log(FString::Printf(TEXT("leaving custom lobby %s."), *Snapshot.Lobby->Id));
	SetBusy(true);
	Call(EVerb::Post, LeaveLobbyPath, FString(), [this](const FVeyraBackendResponse& Response) {
		SetBusy(false);
		if (IsRefusal(Response, TEXT("lobby_locked")))
		{
			// Its champion select started first; the next read shows it.
			return;
		}
		if (!Response.IsSuccess() && !IsRefusal(Response, TEXT("not_in_lobby")))
		{
			ShowRefusal(Response, TEXT("leaving the lobby"), nullptr);
			return;
		}
		EnterShell(FString());
	});
	return true;
}

bool FVeyraClientFlow::KickFromLobby(const FString& AccountId)
{
	if (!CanIssue(EVeyraClientIntent::KickFromLobby) || AccountId == Snapshot.AccountId || !Snapshot.Lobby->FindMember(AccountId))
	{
		return false;
	}
	Log(FString::Printf(TEXT("removing %s from the lobby."), *Snapshot.Lobby->FindMember(AccountId)->DisplayName));
	CallLobby(EVerb::Delete, LobbyMemberPath(AccountId), FString(), TEXT("removing a player from the lobby"));
	return true;
}

bool FVeyraClientFlow::MoveInLobby(const FString& AccountId, const FString& Side, int32 Index)
{
	const VeyraBackendProtocol::FLobbySeat* Target = FindLobbySeat(Side, Index);
	if (!CanIssue(EVeyraClientIntent::MoveInLobby) || !Snapshot.Lobby->FindMember(AccountId) || !Target
		|| Target->Kind != VeyraBackendProtocol::ELobbySeatKind::Empty)
	{
		return false;
	}
	Log(FString::Printf(TEXT("moving %s to seat %d of side %s."), *Snapshot.Lobby->FindMember(AccountId)->DisplayName, Index + 1, *Side));
	CallLobby(EVerb::Put, LobbyMemberPath(AccountId) + TEXT("/seat"), VeyraBackendProtocol::BuildSeatBody(Side, Index), TEXT("moving a player"));
	return true;
}

bool FVeyraClientFlow::SetLobbyBot(const FString& Side, int32 Index, const FString& VanguardId, const FString& Difficulty)
{
	const VeyraBackendProtocol::FLobbySeat* Target = FindLobbySeat(Side, Index);
	if (!CanIssue(EVeyraClientIntent::SetLobbyBot) || !Target || Target->Kind == VeyraBackendProtocol::ELobbySeatKind::Human
		|| !Snapshot.Lobby->BotVanguards.Contains(VanguardId) || !Snapshot.Lobby->BotDifficulties.Contains(Difficulty))
	{
		return false;
	}
	Log(FString::Printf(TEXT("seating a %s %s bot at seat %d of side %s."), *Difficulty, *VanguardId, Index + 1, *Side));
	CallLobby(EVerb::Put, VeyraBackendProtocol::LobbyBotPath(Side, Index), VeyraBackendProtocol::BuildBotBody(VanguardId, Difficulty), TEXT("the bot"));
	return true;
}

bool FVeyraClientFlow::RemoveLobbyBot(const FString& Side, int32 Index)
{
	const VeyraBackendProtocol::FLobbySeat* Target = FindLobbySeat(Side, Index);
	if (!CanIssue(EVeyraClientIntent::RemoveLobbyBot) || !Target || Target->Kind != VeyraBackendProtocol::ELobbySeatKind::Bot)
	{
		return false;
	}
	Log(FString::Printf(TEXT("removing the bot at seat %d of side %s."), Index + 1, *Side));
	CallLobby(EVerb::Delete, VeyraBackendProtocol::LobbyBotPath(Side, Index), FString(), TEXT("removing the bot"));
	return true;
}

bool FVeyraClientFlow::SetLobbySettings(bool bVictoryEnabled, TOptional<double> StartingGold)
{
	if (!CanIssue(EVeyraClientIntent::SetLobbySettings)
		|| (StartingGold.IsSet() && !(StartingGold.GetValue() >= Snapshot.Lobby->StartingGoldMin && StartingGold.GetValue() <= Snapshot.Lobby->StartingGoldMax)))
	{
		return false;
	}
	Log(FString::Printf(TEXT("setting the lobby's rules: victory %s, starting Gold %s."), bVictoryEnabled ? TEXT("on") : TEXT("off"),
		StartingGold.IsSet() ? *FString::SanitizeFloat(StartingGold.GetValue()) : TEXT("the game's own")));
	CallLobby(EVerb::Put, LobbySettingsPath, VeyraBackendProtocol::BuildLobbySettingsBody(bVictoryEnabled, StartingGold), TEXT("the lobby's rules"));
	return true;
}

bool FVeyraClientFlow::LaunchLobby()
{
	if (!CanIssue(EVeyraClientIntent::LaunchLobby))
	{
		return false;
	}
	Log(FString::Printf(TEXT("starting custom lobby %s."), *Snapshot.Lobby->Id));
	CallLobby(EVerb::Post, LaunchLobbyPath, FString(), TEXT("starting the custom match"));
	return true;
}

// Friends and invitations -------------------------------------------------------------------------

void FVeyraClientFlow::PollSocial()
{
	ReadSocial(/*bThenPoll*/ true);
}

void FVeyraClientFlow::ReadSocial(bool bThenPoll)
{
	const uint32 Sequence = ++SocialSequence;
	const auto Next = [this, bThenPoll] {
		if (bThenPoll)
		{
			After(Config.SocialPollIntervalSeconds, [this] { PollSocial(); });
		}
	};
	Probe(EVerb::Get, FriendsPath, [this, Sequence, Next](const FVeyraBackendResponse& Response) {
		VeyraBackendProtocol::FFriends Friends;
		FString Problem;
		if (!Response.IsSuccess() || !VeyraBackendProtocol::ParseFriends(Response.Body, Friends, Problem))
		{
			// The panel keeps what it last read; the next read may do better.
			Log(FString::Printf(TEXT("could not read the friends: %s."), Response.IsSuccess() ? *Problem : *Response.Describe()));
			Next();
			return;
		}
		Probe(EVerb::Get, LobbyInvitesPath, [this, Sequence, Next, Friends = MoveTemp(Friends)](const FVeyraBackendResponse& InvitesResponse) mutable {
			TArray<VeyraBackendProtocol::FLobbyInvite> Invites;
			FString InvitesProblem;
			// A backend with custom lobbies switched off has no invitations to offer.
			if (InvitesResponse.Status != NotFoundStatus
				&& (!InvitesResponse.IsSuccess() || !VeyraBackendProtocol::ParseLobbyInvites(InvitesResponse.Body, Invites, InvitesProblem)))
			{
				Log(FString::Printf(TEXT("could not read the lobby invitations: %s."), InvitesResponse.IsSuccess() ? *InvitesProblem : *InvitesResponse.Describe()));
				Invites = Snapshot.Social.LobbyInvites;
			}
			if (Sequence >= ShownSocialSequence)
			{
				ShownSocialSequence = Sequence;
				FVeyraSocial& Social = Snapshot.Social;
				if (!Social.bLoaded || !(Social.Friends == Friends) || !(Social.LobbyInvites == Invites))
				{
					Social.bLoaded = true;
					Social.Friends = MoveTemp(Friends);
					Social.LobbyInvites = MoveTemp(Invites);
					Broadcast();
				}
			}
			Next();
		});
	});
}

void FVeyraClientFlow::CallSocial(EVerb Verb, const FString& Path, const FString& Body, const FString& Name, TFunction<void(const FVeyraBackendResponse&)> OnSuccess)
{
	SetBusy(true);
	Call(Verb, Path, Body, [this, Name, OnSuccess = MoveTemp(OnSuccess)](const FVeyraBackendResponse& Response) {
		SetBusy(false);
		if (!Response.IsSuccess())
		{
			ShowSocialFeedback(RefusalCode(Response), Name);
		}
		else if (OnSuccess)
		{
			OnSuccess(Response);
		}
		ReadSocial(/*bThenPoll*/ false);
	});
}

void FVeyraClientFlow::ShowSocialFeedback(const FString& Code, const FString& Name)
{
	Log(FString::Printf(TEXT("friends panel: %s (%s)."), *Code, *Name));
	Snapshot.Social.Feedback = Code;
	Snapshot.Social.FeedbackName = Name;
	Broadcast();
}

bool FVeyraClientFlow::SendFriendRequest(const FString& DisplayName)
{
	const FString Name = DisplayName.TrimStartAndEnd();
	if (!CanIssue(EVeyraClientIntent::SendFriendRequest) || Name.IsEmpty())
	{
		return false;
	}
	if (Name == Snapshot.DisplayName)
	{
		ShowSocialFeedback(SelfFeedback, Name);
		return true;
	}
	Log(FString::Printf(TEXT("asking %s to be friends."), *Name));
	SetBusy(true);
	// Players find each other by display name (Parties & Social Bible §1).
	Call(EVerb::Get, VeyraBackendProtocol::AccountLookupPath(Name), FString(), [this, Name](const FVeyraBackendResponse& Response) {
		VeyraBackendProtocol::FAccount Account;
		FString Problem;
		if (!Response.IsSuccess())
		{
			SetBusy(false);
			ShowSocialFeedback(RefusalCode(Response), Name);
			return;
		}
		if (!VeyraBackendProtocol::ParseAccount(Response.Body, Account, Problem))
		{
			SetBusy(false);
			ShowBadAnswer(TEXT("the account"), Problem, nullptr);
			return;
		}
		CallSocial(EVerb::Post, FriendRequestsPath, VeyraBackendProtocol::BuildAccountBody(Account.Id), Account.DisplayName,
			[this, Name = Account.DisplayName](const FVeyraBackendResponse& RequestResponse) {
				FString Outcome;
				FString OutcomeProblem;
				if (!VeyraBackendProtocol::ParseFriendRequestOutcome(RequestResponse.Body, Outcome, OutcomeProblem))
				{
					ShowBadAnswer(TEXT("the friend request"), OutcomeProblem, nullptr);
					return;
				}
				ShowSocialFeedback(Outcome == BecameFriendsOutcome ? FriendAddedFeedback : FriendRequestedFeedback, Name);
			});
	});
	return true;
}

bool FVeyraClientFlow::AnswerFriendRequest(const FString& AccountId, bool bAccept)
{
	const VeyraBackendProtocol::FAccount* From = Snapshot.Social.Friends.Incoming.FindByPredicate(
		[&AccountId](const VeyraBackendProtocol::FAccount& Candidate) { return Candidate.Id == AccountId; });
	if (!CanIssue(EVeyraClientIntent::AnswerFriendRequest) || !From)
	{
		return false;
	}
	Log(FString::Printf(TEXT("%s %s's friend request."), bAccept ? TEXT("accepting") : TEXT("declining"), *From->DisplayName));
	CallSocial(EVerb::Post, FriendRequestPath(AccountId, bAccept ? TEXT("accept") : TEXT("decline")), FString(), From->DisplayName,
		[this, bAccept, Name = From->DisplayName](const FVeyraBackendResponse&) {
			if (bAccept)
			{
				ShowSocialFeedback(FriendAddedFeedback, Name);
			}
		});
	return true;
}

bool FVeyraClientFlow::RemoveFriend(const FString& AccountId)
{
	const VeyraBackendProtocol::FAccount* Friend = Snapshot.Social.Friends.Friends.FindByPredicate(
		[&AccountId](const VeyraBackendProtocol::FAccount& Candidate) { return Candidate.Id == AccountId; });
	if (!CanIssue(EVeyraClientIntent::RemoveFriend) || !Friend)
	{
		return false;
	}
	Log(FString::Printf(TEXT("removing %s from the friends."), *Friend->DisplayName));
	CallSocial(EVerb::Delete, FString(FriendsPath) + TEXT("/") + AccountId, FString(), Friend->DisplayName, nullptr);
	return true;
}

// A match found ---------------------------------------------------------------------------------

void FVeyraClientFlow::EnterMatchFound(const VeyraBackendProtocol::FMatchFound& Found)
{
	Enter(EVeyraClientState::MatchFound);
	Log(FString::Printf(TEXT("a match was found (%s, %s): %d of %d accepted."), *Found.Id, *Found.Mode, Found.Accepted, Found.Total));
	ApplyMatchFound(Found);
	if (Snapshot.State == EVeyraClientState::MatchFound)
	{
		After(Config.MatchFoundPollIntervalSeconds, [this] { PollMatchFound(); });
	}
}

void FVeyraClientFlow::PollMatchFound()
{
	Call(EVerb::Get, MatchFoundPath, FString(), [this](const FVeyraBackendResponse& Response) {
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("the match found"), [this] { PollMatchFound(); });
			return;
		}
		TOptional<VeyraBackendProtocol::FMatchFound> Found;
		FString Problem;
		if (!VeyraBackendProtocol::ParseMatchFound(Response.Body, Found, Problem))
		{
			ShowBadAnswer(TEXT("the match found"), Problem, [this] { PollMatchFound(); });
			return;
		}
		if (!Found.IsSet() || Found->Id != Snapshot.MatchFound.Id)
		{
			// Only a match found that waits for answers is reported: this one is over.
			LeaveMatchFound();
			return;
		}
		ApplyMatchFound(*Found);
		if (Snapshot.State == EVeyraClientState::MatchFound)
		{
			After(Config.MatchFoundPollIntervalSeconds, [this] { PollMatchFound(); });
		}
	});
}

void FVeyraClientFlow::ApplyMatchFound(const VeyraBackendProtocol::FMatchFound& Found)
{
	Snapshot.MatchFound = Found;
	if (Found.State != PendingAnswer)
	{
		LeaveMatchFound();
		return;
	}
	Snapshot.AcceptEndsAt = Host.Now() + Found.RemainingSeconds;
	Broadcast();
}

void FVeyraClientFlow::LeaveMatchFound()
{
	// An accepted match continues in its select. Otherwise, the notice says why the player is back
	// in the shell, by their own answer and then their party: nobody learns who else declined
	// (Parties & Social Bible §3).
	const VeyraBackendProtocol::FMatchFound& Found = Snapshot.MatchFound;
	FString Notice;
	if (Found.State != AcceptedAnswer)
	{
		Notice = Found.You == DeclinedAnswer ? DeclinedNotice : Found.You == AcceptedAnswer ? AbandonedNotice : MissedNotice;
	}
	Log(FString::Printf(TEXT("match found %s is over (%s)."), *Found.Id, Notice.IsEmpty() ? AcceptedAnswer : *Notice));
	Resume(Notice);
}

bool FVeyraClientFlow::AcceptMatch()
{
	if (!CanIssue(EVeyraClientIntent::AcceptMatch))
	{
		return false;
	}
	AnswerMatchFound(/*bAccept*/ true);
	return true;
}

bool FVeyraClientFlow::DeclineMatch()
{
	if (!CanIssue(EVeyraClientIntent::DeclineMatch))
	{
		return false;
	}
	AnswerMatchFound(/*bAccept*/ false);
	return true;
}

void FVeyraClientFlow::AnswerMatchFound(bool bAccept)
{
	Log(bAccept ? TEXT("accepting the match.") : TEXT("declining the match."));
	SetBusy(true);
	Call(EVerb::Post, bAccept ? AcceptMatchPath : DeclineMatchPath, FString(), [this](const FVeyraBackendResponse& Response) {
		SetBusy(false);
		TOptional<VeyraBackendProtocol::FMatchFound> Found;
		FString Problem;
		if (Response.IsSuccess() && VeyraBackendProtocol::ParseMatchFound(Response.Body, Found, Problem) && Found.IsSet() && Found->Id == Snapshot.MatchFound.Id)
		{
			// The last acceptance opens the select before the backend answers.
			ApplyMatchFound(*Found);
		}
		else if (Response.IsSuccess())
		{
			ShowBadAnswer(TEXT("the answer to the match found"), Problem.IsEmpty() ? FString(TEXT("it is not the player's match found")) : Problem, nullptr);
		}
		else if (IsRefusal(Response, TEXT("match_found_not_found")) || IsRefusal(Response, TEXT("match_found_over")) || IsRefusal(Response, TEXT("expired")))
		{
			LeaveMatchFound();
		}
		else if (!IsRefusal(Response, TEXT("already_answered")))
		{
			ShowRefusal(Response, TEXT("the answer to the match found"), nullptr);
		}
		// An answer an earlier attempt already gave shows on the next read.
	});
}

// Champion select -------------------------------------------------------------------------------

void FVeyraClientFlow::EnterSelecting(const VeyraBackendProtocol::FSelect& Select)
{
	Enter(EVeyraClientState::Selecting);
	SelectId = Select.Id;
	Snapshot.AvailableVanguards.Reset();
	Snapshot.ReleasedVanguards.Reset();
	Log(FString::Printf(TEXT("in champion select %s (%s)."), *Select.Id, *Select.Mode));
	ApplySelect(Select);
	if (Snapshot.State != EVeyraClientState::Selecting)
	{
		return;
	}
	LoadAvailableVanguards();
	After(Config.SelectPollIntervalSeconds, [this] { PollSelect(); });
}

void FVeyraClientFlow::ApplySelect(const VeyraBackendProtocol::FSelect& Select)
{
	switch (Select.State)
	{
	case ESelectState::Started:
		EnterMatchStarting(Select.MatchId);
		return;
	case ESelectState::Cancelled:
		Log(FString::Printf(TEXT("champion select %s was cancelled (%s)."), *Select.Id, *Select.CancelReason));
		if (Select.Kind == CustomSelectKind)
		{
			// Its lobby opens again (ADR-021 §3), which finding where the player is leads back to.
			Resume(Select.CancelReason);
			return;
		}
		EnterShell(Select.CancelReason);
		return;
	default:
		Snapshot.Select = Select;
		Snapshot.PickEndsAt = Host.Now() + Select.RemainingSeconds;
		Broadcast();
		return;
	}
}

void FVeyraClientFlow::LoadAvailableVanguards()
{
	Call(EVerb::Get, VanguardsPath, FString(), [this](const FVeyraBackendResponse& Response) {
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("the Vanguards the player may pick"), [this] { LoadAvailableVanguards(); });
			return;
		}
		VeyraBackendProtocol::FVanguardAccess Access;
		FString Problem;
		if (!VeyraBackendProtocol::ParseVanguardAccess(Response.Body, Access, Problem))
		{
			ShowBadAnswer(TEXT("the Vanguards the player may pick"), Problem, [this] { LoadAvailableVanguards(); });
			return;
		}
		Snapshot.AvailableVanguards = MoveTemp(Access.Available);
		Snapshot.ReleasedVanguards = MoveTemp(Access.Released);
		Broadcast();
	});
}

void FVeyraClientFlow::PollSelect()
{
	Call(EVerb::Get, MySelectPath, FString(), [this](const FVeyraBackendResponse& Response) {
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("the champion select"), [this] { PollSelect(); });
			return;
		}
		TOptional<VeyraBackendProtocol::FSelect> Select;
		FString Problem;
		if (!VeyraBackendProtocol::ParseSelect(Response.Body, Select, Problem))
		{
			ShowBadAnswer(TEXT("the champion select"), Problem, [this] { PollSelect(); });
			return;
		}
		if (!Select.IsSet() || Select->Id != SelectId)
		{
			// The select is over.
			LearnHowSelectEnded();
			return;
		}
		ApplySelect(*Select);
		if (Snapshot.State == EVeyraClientState::Selecting)
		{
			After(Config.SelectPollIntervalSeconds, [this] { PollSelect(); });
		}
	});
}

void FVeyraClientFlow::LearnHowSelectEnded()
{
	Call(EVerb::Get, SelectPath(SelectId), FString(), [this](const FVeyraBackendResponse& Response) {
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("how the champion select ended"), [this] { LearnHowSelectEnded(); });
			return;
		}
		TOptional<VeyraBackendProtocol::FSelect> Select;
		FString Problem;
		if (!VeyraBackendProtocol::ParseSelect(Response.Body, Select, Problem) || !Select.IsSet() || Select->Id != SelectId)
		{
			ShowBadAnswer(TEXT("how the champion select ended"), Problem.IsEmpty() ? FString(TEXT("it is not the player's select")) : Problem,
				[this] { LearnHowSelectEnded(); });
			return;
		}
		ApplySelect(*Select);
		if (Snapshot.State == EVeyraClientState::Selecting)
		{
			After(Config.SelectPollIntervalSeconds, [this] { PollSelect(); });
		}
	});
}

bool FVeyraClientFlow::HoverVanguard(const FString& VanguardId)
{
	if (!CanIssue(EVeyraClientIntent::HoverVanguard) || !Snapshot.AvailableVanguards.Contains(VanguardId))
	{
		return false;
	}
	SetBusy(true);
	Call(EVerb::Put, HoverPath, VeyraBackendProtocol::BuildVanguardBody(VanguardId), [this](const FVeyraBackendResponse& Response) {
		SetBusy(false);
		TOptional<VeyraBackendProtocol::FSelect> Select;
		FString Problem;
		if (Response.IsSuccess() && VeyraBackendProtocol::ParseSelect(Response.Body, Select, Problem) && Select.IsSet() && Select->Id == SelectId)
		{
			ApplySelect(*Select);
		}
		else if (Response.IsSuccess())
		{
			ShowBadAnswer(TEXT("the hover"), Problem.IsEmpty() ? FString(TEXT("it is not the player's select")) : Problem, nullptr);
		}
		else if (IsRefusal(Response, TEXT("not_available")) || IsRefusal(Response, TEXT("taken")))
		{
			ShowRefusal(Response, TEXT("the hover"), nullptr);
		}
		// Otherwise the select moved on, and the next read shows where.
	});
	return true;
}

bool FVeyraClientFlow::LockVanguard(const FString& VanguardId)
{
	if (!CanIssue(EVeyraClientIntent::LockVanguard) || !Snapshot.AvailableVanguards.Contains(VanguardId))
	{
		return false;
	}
	Log(FString::Printf(TEXT("locking in %s."), *VanguardId));
	SetBusy(true);
	Call(EVerb::Post, LockPath, VeyraBackendProtocol::BuildVanguardBody(VanguardId), [this](const FVeyraBackendResponse& Response) {
		SetBusy(false);
		TOptional<VeyraBackendProtocol::FSelect> Select;
		FString Problem;
		if (Response.IsSuccess() && VeyraBackendProtocol::ParseSelect(Response.Body, Select, Problem) && Select.IsSet() && Select->Id == SelectId)
		{
			// Once every pick is locked the backend creates the match before it answers.
			ApplySelect(*Select);
		}
		else if (Response.IsSuccess())
		{
			ShowBadAnswer(TEXT("the lock"), Problem.IsEmpty() ? FString(TEXT("it is not the player's select")) : Problem, nullptr);
		}
		else if (IsRefusal(Response, TEXT("not_available")) || IsRefusal(Response, TEXT("taken")))
		{
			// In a matchmade select a Vanguard is unique: another player may have locked it first.
			ShowRefusal(Response, TEXT("the lock"), nullptr);
		}
		// Otherwise, such as a lock that an earlier attempt already made, the next read shows where the select is.
	});
	return true;
}

bool FVeyraClientFlow::ChooseFluxSpell(int32 Slot, const FString& SpellId)
{
	constexpr int32 SlotCount = static_cast<int32>(UE_ARRAY_COUNT(VeyraAbilitySlots::Spells));
	const VeyraBackendProtocol::FSelectSeat* You = Snapshot.Select.FindYou();
	if (!CanIssue(EVeyraClientIntent::ChooseFluxSpell) || !You || Slot < 0 || Slot >= SlotCount)
	{
		return false;
	}
	TArray<FString> Spells = You->FluxSpells;
	Spells.SetNum(SlotCount);
	// The other slot's spell moves over to this slot's old place.
	const int32 Elsewhere = SpellId.IsEmpty() ? INDEX_NONE : Spells.IndexOfByKey(SpellId);
	if (Elsewhere != INDEX_NONE && Elsewhere != Slot)
	{
		Spells[Elsewhere] = Spells[Slot];
	}
	Spells[Slot] = SpellId;
	SetBusy(true);
	Call(EVerb::Put, FluxSpellsPath, VeyraBackendProtocol::BuildFluxSpellsBody(Spells), [this](const FVeyraBackendResponse& Response) {
		SetBusy(false);
		TOptional<VeyraBackendProtocol::FSelect> Select;
		FString Problem;
		if (Response.IsSuccess() && VeyraBackendProtocol::ParseSelect(Response.Body, Select, Problem) && Select.IsSet() && Select->Id == SelectId)
		{
			ApplySelect(*Select);
		}
		else if (Response.IsSuccess())
		{
			ShowBadAnswer(TEXT("the Flux Spells"), Problem.IsEmpty() ? FString(TEXT("it is not the player's select")) : Problem, nullptr);
		}
		else if (IsRefusal(Response, TEXT("invalid_flux_spells")))
		{
			ShowRefusal(Response, TEXT("the Flux Spells"), nullptr);
		}
		// Otherwise the select moved on, and the next read shows where.
	});
	return true;
}

bool FVeyraClientFlow::LeaveSelect()
{
	if (!CanIssue(EVeyraClientIntent::LeaveSelect))
	{
		return false;
	}
	Log(FString::Printf(TEXT("leaving champion select %s."), *SelectId));
	SetBusy(true);
	Call(EVerb::Post, LeaveSelectPath, FString(), [this](const FVeyraBackendResponse& Response) {
		SetBusy(false);
		TOptional<VeyraBackendProtocol::FSelect> Select;
		FString Problem;
		if (Response.IsSuccess() && VeyraBackendProtocol::ParseSelect(Response.Body, Select, Problem) && Select.IsSet() && Select->Id == SelectId)
		{
			if (Select->State == ESelectState::Cancelled && Select->Kind == CustomSelectKind)
			{
				// Everyone, the leaver too, goes back to the lobby.
				Resume(YouLeftCustomNotice);
				return;
			}
			if (Select->State == ESelectState::Cancelled)
			{
				// Everyone else sees it cancelled because a player left; the leaver's party left the queue.
				EnterShell(TEXT("you_left"));
				return;
			}
			ApplySelect(*Select);
		}
		else if (Response.IsSuccess())
		{
			ShowBadAnswer(TEXT("leaving champion select"), Problem.IsEmpty() ? FString(TEXT("it is not the player's select")) : Problem, nullptr);
		}
		else if (IsRefusal(Response, TEXT("cannot_leave")))
		{
			ShowRefusal(Response, TEXT("leaving champion select"), nullptr);
		}
		// Otherwise the select moved on, and the next read shows where.
	});
	return true;
}

bool FVeyraClientFlow::HoverBan(const FString& VanguardId)
{
	if (!CanIssue(EVeyraClientIntent::HoverBan) || !Snapshot.ReleasedVanguards.Contains(VanguardId) || Snapshot.Select.IsBanned(VanguardId))
	{
		return false;
	}
	SendSelectAction(EVerb::Put, BanHoverPath, VeyraBackendProtocol::BuildVanguardBody(VanguardId), TEXT("the ban hover"),
		{ TEXT("not_available"), TEXT("taken") });
	return true;
}

bool FVeyraClientFlow::BanVanguard(const FString& VanguardId)
{
	if (!CanIssue(EVeyraClientIntent::BanVanguard) || !Snapshot.ReleasedVanguards.Contains(VanguardId) || Snapshot.Select.IsBanned(VanguardId))
	{
		return false;
	}
	Log(FString::Printf(TEXT("banning %s."), *VanguardId));
	// Another ban may have taken it first; a turn that ran out shows on the next read.
	SendSelectAction(EVerb::Post, BanPath, VeyraBackendProtocol::BuildVanguardBody(VanguardId), TEXT("the ban"), { TEXT("not_available"), TEXT("taken") });
	return true;
}

bool FVeyraClientFlow::OfferTrade(int32 Seat)
{
	const VeyraBackendProtocol::FSelectSeat* You = LockedTrader(Snapshot.Select);
	if (!CanIssue(EVeyraClientIntent::OfferTrade) || !You || !Snapshot.Select.Seats.IsValidIndex(Seat))
	{
		return false;
	}
	const VeyraBackendProtocol::FSelectSeat& Teammate = Snapshot.Select.Seats[Seat];
	if (Teammate.bYou || Teammate.Side != You->Side || Teammate.Locked.IsEmpty())
	{
		return false;
	}
	Log(FString::Printf(TEXT("offering %s a trade: %s for %s."), *Teammate.DisplayName, *You->Locked, *Teammate.Locked));
	SendSelectAction(EVerb::Post, TradePath, VeyraBackendProtocol::BuildTradeBody(Seat), TEXT("the trade offer"), { TEXT("cannot_trade") });
	return true;
}

bool FVeyraClientFlow::AnswerTrade(int32 Seat, bool bAccept)
{
	if (!CanIssue(EVeyraClientIntent::AnswerTrade) || !Snapshot.Select.Seats.IsValidIndex(Seat) || !Snapshot.Select.Seats[Seat].bOffersYou)
	{
		return false;
	}
	Log(FString::Printf(TEXT("%s %s's trade."), bAccept ? TEXT("accepting") : TEXT("declining"), *Snapshot.Select.Seats[Seat].DisplayName));
	// A trade that would leave either player a Vanguard they may not play is refused; an offer that
	// lapsed meanwhile shows on the next read.
	SendSelectAction(EVerb::Post, bAccept ? AcceptTradePath : DeclineTradePath, VeyraBackendProtocol::BuildTradeBody(Seat),
		bAccept ? TEXT("accepting the trade") : TEXT("declining the trade"), { TEXT("not_available") });
	return true;
}

void FVeyraClientFlow::SendSelectAction(EVerb Verb, const TCHAR* Path, const FString& Body, const TCHAR* What, TArray<const TCHAR*> ShownRefusals)
{
	SetBusy(true);
	Call(Verb, Path, Body, [this, What, ShownRefusals = MoveTemp(ShownRefusals)](const FVeyraBackendResponse& Response) {
		SetBusy(false);
		TOptional<VeyraBackendProtocol::FSelect> Select;
		FString Problem;
		if (Response.IsSuccess() && VeyraBackendProtocol::ParseSelect(Response.Body, Select, Problem) && Select.IsSet() && Select->Id == SelectId)
		{
			ApplySelect(*Select);
		}
		else if (Response.IsSuccess())
		{
			ShowBadAnswer(What, Problem.IsEmpty() ? FString(TEXT("it is not the player's select")) : Problem, nullptr);
		}
		else if (ShownRefusals.ContainsByPredicate([&Response](const TCHAR* Code) { return IsRefusal(Response, Code); }))
		{
			ShowRefusal(Response, What, nullptr);
		}
		// Otherwise the select moved on, and the next read shows where.
	});
}

// The match -------------------------------------------------------------------------------------

void FVeyraClientFlow::EnterMatchStarting(const FString& MatchId)
{
	Enter(EVeyraClientState::MatchStarting);
	Snapshot.MatchId = MatchId;
	bMatchEnded = false;
	MatchDeadline = Host.Now() + Config.MatchWaitTimeoutSeconds;
	Log(FString::Printf(TEXT("match %s is starting."), *MatchId));
	Broadcast();
	PollMatch();
}

void FVeyraClientFlow::PollMatch()
{
	Call(EVerb::Get, MyMatchPath, FString(), [this](const FVeyraBackendResponse& Response) {
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("the player's match"), [this] { PollMatch(); });
			return;
		}
		VeyraBackendProtocol::FMyMatch Match;
		FString Problem;
		if (!VeyraBackendProtocol::ParseMyMatch(Response.Body, Match, Problem))
		{
			ShowBadAnswer(TEXT("the player's match"), Problem, [this] { PollMatch(); });
			return;
		}
		if (!Match.bHasMatch)
		{
			// It ended before its server was ready; its outcome says why.
			bMatchEnded = true;
			EnterAwaitingResults();
			return;
		}
		Snapshot.MatchId = Match.MatchId;
		if (Match.bReady)
		{
			Connect(Match);
			return;
		}
		if (Host.Now() >= MatchDeadline)
		{
			ShowProblem(TEXT("match_not_ready"), TEXT("the match's server was not ready within MatchWaitTimeoutSeconds"), [this] {
				MatchDeadline = Host.Now() + Config.MatchWaitTimeoutSeconds;
				PollMatch();
			});
			return;
		}
		After(Config.MatchPollIntervalSeconds, [this] { PollMatch(); });
	});
}

void FVeyraClientFlow::Connect(const VeyraBackendProtocol::FMyMatch& Match)
{
	Enter(EVeyraClientState::Connecting);
	Snapshot.MatchId = Match.MatchId;
	bMatchEnded = false;
	const FString Address = FString::Printf(TEXT("%s:%d"), *Match.Host, Match.Port);
	Log(FString::Printf(TEXT("joining match %s at %s."), *Match.MatchId, *Address));
	if (!Host.TravelToMatch(Address, Match.Ticket))
	{
		ShowProblem(TEXT("travel_failed"), TEXT("the game could not travel to the match server"), nullptr);
		return;
	}
	Broadcast();
}

void FVeyraClientFlow::NotifyWorld(EVeyraClientWorld World)
{
	switch (Snapshot.State)
	{
	case EVeyraClientState::Connecting:
		if (World == EVeyraClientWorld::Match)
		{
			Enter(EVeyraClientState::InMatch);
			Log(FString::Printf(TEXT("in match %s."), *Snapshot.MatchId));
			Broadcast();
		}
		else
		{
			// The engine returns to the front end when a join fails.
			LeaveMatch(/*bEnded*/ false, TEXT("join_failed"));
		}
		break;
	case EVeyraClientState::InMatch:
		if (World == EVeyraClientWorld::FrontEnd)
		{
			LeaveMatch(/*bEnded*/ false, TEXT("connection_lost"));
		}
		break;
	case EVeyraClientState::Returning:
		if (World == EVeyraClientWorld::FrontEnd)
		{
			EnterAwaitingResults();
		}
		break;
	default:
		break;
	}
}

void FVeyraClientFlow::NotifyMatchPhase(EVeyraMatchPhase Phase)
{
	if (Phase == EVeyraMatchPhase::Ended && !bWatchingEnd && (Snapshot.State == EVeyraClientState::InMatch || Snapshot.State == EVeyraClientState::Connecting))
	{
		// The replicated end is not the result: the backend's verified one is (UX-15). The player first
		// watches the match end, while its server stays up for it (ADR-020 §1).
		bWatchingEnd = true;
		After(Config.EndingShowSeconds, [this] { LeaveMatch(/*bEnded*/ true, FString()); });
	}
}

void FVeyraClientFlow::NotifyConnectionFailed(const FString& Reason)
{
	// A server that quits as its players finish watching the end ended the match; nothing failed.
	if (bWatchingEnd && Snapshot.State == EVeyraClientState::InMatch)
	{
		LeaveMatch(/*bEnded*/ true, FString());
		return;
	}
	if (Snapshot.State == EVeyraClientState::InMatch || Snapshot.State == EVeyraClientState::Connecting)
	{
		UE_LOG(LogVeyraServices, Warning, TEXT("VeyraClientFlow: the connection to match %s failed: %s."), *Snapshot.MatchId,
			*VeyraBackendProtocol::RedactCredentials(Reason));
		LeaveMatch(/*bEnded*/ false, Snapshot.State == EVeyraClientState::Connecting ? TEXT("join_failed") : TEXT("connection_lost"));
	}
}

void FVeyraClientFlow::LeaveMatch(bool bEnded, const FString& Notice)
{
	bWatchingEnd = false;
	bMatchEnded = bEnded;
	Enter(EVeyraClientState::Returning, Notice);
	Log(FString::Printf(TEXT("leaving match %s%s."), *Snapshot.MatchId, bEnded ? TEXT(", which ended") : TEXT(", whose connection failed")));
	Host.TravelToFrontEnd();
	Broadcast();
}

void FVeyraClientFlow::EnterAwaitingResults()
{
	// A lost connection's notice stays, so the results screen can say why the player is there.
	const FString Notice = Snapshot.Notice;
	Enter(EVeyraClientState::AwaitingResults, Notice);
	ResultDeadline = Host.Now() + Config.ResultWaitTimeoutSeconds;
	Broadcast();
	PollResult();
}

void FVeyraClientFlow::PollResult()
{
	Call(EVerb::Get, MatchOutcomePath(Snapshot.MatchId), FString(), [this](const FVeyraBackendResponse& Response) {
		if (IsRefusal(Response, TEXT("match_not_found")))
		{
			ShowResults({});
			return;
		}
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("the match's result"), [this] { PollResult(); });
			return;
		}
		VeyraBackendProtocol::FMatchOutcome Outcome;
		FString Problem;
		if (!VeyraBackendProtocol::ParseMatchOutcome(Response.Body, Outcome, Problem))
		{
			ShowBadAnswer(TEXT("the match's result"), Problem, [this] { PollResult(); });
			return;
		}
		if (!Outcome.IsActive())
		{
			ShowResults(MoveTemp(Outcome));
			return;
		}
		if (!bMatchEnded)
		{
			// The player lost a match that still runs (UX-17).
			EnterReconnectOnly(Outcome.MatchId);
			return;
		}
		if (Host.Now() >= ResultDeadline)
		{
			ShowResults({});
			return;
		}
		After(Config.ResultPollIntervalSeconds, [this] { PollResult(); });
	});
}

void FVeyraClientFlow::ShowResults(TOptional<VeyraBackendProtocol::FMatchOutcome> Outcome)
{
	const FString Notice = Snapshot.Notice;
	Enter(EVeyraClientState::Results, Notice);
	if (Outcome.IsSet() && Outcome->bHasResult)
	{
		Log(FString::Printf(TEXT("match %s ended: %s, winner %s, %.1f s, as %s."), *Outcome->MatchId, *Outcome->EndReason,
			Outcome->Winner.IsEmpty() ? TEXT("none") : *Outcome->Winner, Outcome->DurationSeconds, *Outcome->VanguardId));
	}
	else if (Outcome.IsSet())
	{
		Log(FString::Printf(TEXT("match %s failed: %s."), *Outcome->MatchId, *Outcome->FailureReason));
	}
	else
	{
		Log(FString::Printf(TEXT("match %s has no verified result yet."), *Snapshot.MatchId));
	}
	Snapshot.Result = MoveTemp(Outcome);
	Broadcast();
}

bool FVeyraClientFlow::ContinueFromResults()
{
	if (!CanIssue(EVeyraClientIntent::ContinueFromResults))
	{
		return false;
	}
	Snapshot.Result.Reset();
	Resume();
	return true;
}

void FVeyraClientFlow::EnterReconnectOnly(const FString& MatchId)
{
	const FString Notice = Snapshot.Notice;
	Enter(EVeyraClientState::ReconnectOnly, Notice);
	Snapshot.MatchId = MatchId;
	bMatchEnded = false;
	Log(FString::Printf(TEXT("match %s runs; the player may only reconnect."), *MatchId));
	Broadcast();
	After(Config.ReconnectPollIntervalSeconds, [this] { PollReconnect(); });
}

void FVeyraClientFlow::PollReconnect()
{
	Call(EVerb::Get, MyMatchPath, FString(), [this](const FVeyraBackendResponse& Response) {
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("the player's match"), [this] { PollReconnect(); });
			return;
		}
		VeyraBackendProtocol::FMyMatch Match;
		FString Problem;
		if (!VeyraBackendProtocol::ParseMyMatch(Response.Body, Match, Problem))
		{
			ShowBadAnswer(TEXT("the player's match"), Problem, [this] { PollReconnect(); });
			return;
		}
		if (!Match.bHasMatch || Match.MatchId != Snapshot.MatchId)
		{
			// It ended without the player: show its result.
			bMatchEnded = true;
			EnterAwaitingResults();
			return;
		}
		After(Config.ReconnectPollIntervalSeconds, [this] { PollReconnect(); });
	});
}

bool FVeyraClientFlow::Reconnect()
{
	if (!CanIssue(EVeyraClientIntent::Reconnect))
	{
		return false;
	}
	Log(FString::Printf(TEXT("reconnecting to match %s."), *Snapshot.MatchId));
	SetBusy(true);
	Call(EVerb::Get, MyMatchPath, FString(), [this](const FVeyraBackendResponse& Response) {
		SetBusy(false);
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("the player's match"), nullptr);
			return;
		}
		VeyraBackendProtocol::FMyMatch Match;
		FString Problem;
		if (!VeyraBackendProtocol::ParseMyMatch(Response.Body, Match, Problem))
		{
			ShowBadAnswer(TEXT("the player's match"), Problem, nullptr);
			return;
		}
		if (!Match.bHasMatch)
		{
			bMatchEnded = true;
			EnterAwaitingResults();
		}
		else if (Match.bReady)
		{
			Connect(Match);
		}
		else
		{
			EnterMatchStarting(Match.MatchId);
		}
	});
	return true;
}

// Every state -----------------------------------------------------------------------------------

bool FVeyraClientFlow::Retry()
{
	if (!CanIssue(EVeyraClientIntent::Retry) || !RetryAction)
	{
		return false;
	}
	TFunction<void()> Step = MoveTemp(RetryAction);
	RetryAction = nullptr;
	Snapshot.Problem.Reset();
	Log(TEXT("retrying."));
	Broadcast();
	Step();
	return true;
}

bool FVeyraClientFlow::ResolveSettingsConflict(bool bKeepThisDevice)
{
	if (!CanIssue(EVeyraClientIntent::ResolveSettingsConflict) || !AccountSettings)
	{
		return false;
	}
	Log(bKeepThisDevice ? TEXT("keeping this device's settings.") : TEXT("taking the account's settings."));
	return AccountSettings->Resolve(bKeepThisDevice);
}

bool FVeyraClientFlow::Quit()
{
	Log(TEXT("the player quits."));
	Host.QuitGame();
	return true;
}

void FVeyraClientFlow::EndSession()
{
	GameSession.Reset();
	if (AccountSettings)
	{
		AccountSettings->SignOut();
		Snapshot.bSettingsConflict = false;
	}
	Enter(EVeyraClientState::SessionEnded);
	Snapshot.Problem = FVeyraClientProblem{ TEXT("session_ended"), TEXT("the backend no longer accepts this game session; sign in again from the launcher"), false };
	UE_LOG(LogVeyraServices, Warning, TEXT("VeyraClientFlow: the backend ended the game session."));
	Broadcast();
}

void FVeyraClientFlow::Enter(EVeyraClientState NewState, const FString& Notice)
{
	++Epoch;
	Waits.Reset();
	RetryAction = nullptr;
	Snapshot.State = NewState;
	Snapshot.bBusy = false;
	Snapshot.Problem.Reset();
	Snapshot.Notice = Notice;
	Log(FString::Printf(TEXT("%s%s%s."), LexToString(NewState), Notice.IsEmpty() ? TEXT("") : TEXT(": "), *Notice));
}

void FVeyraClientFlow::After(double Seconds, TFunction<void()> Run)
{
	Waits.Add(FWait{ Host.Now() + Seconds, Epoch, MoveTemp(Run) });
}

void FVeyraClientFlow::Call(EVerb Verb, const FString& Path, const FString& Body, FAnswer OnAnswer, int32 Attempt)
{
	FVeyraBackendCallback OnDone = [this, WeakAlive = TWeakPtr<bool>(Alive), CallEpoch = Epoch, Verb, Path, Body, OnAnswer = MoveTemp(OnAnswer),
										  Attempt](const FVeyraBackendResponse& Response) mutable {
		if (!WeakAlive.IsValid() || CallEpoch != Epoch)
		{
			return;
		}
		if (Response.IsUnauthorized())
		{
			EndSession();
			return;
		}
		if (!Response.IsTransient())
		{
			OnAnswer(Response);
			return;
		}
		if (Attempt < Config.RequestAttempts)
		{
			After(Config.RetryIntervalSeconds, [this, Verb, Path, Body, OnAnswer = MoveTemp(OnAnswer), Attempt]() mutable {
				Call(Verb, Path, Body, MoveTemp(OnAnswer), Attempt + 1);
			});
			return;
		}
		SetBusy(false);
		ShowProblem(TEXT("backend_unreachable"), FString::Printf(TEXT("the backend did not answer %s after %d attempt(s): %s"), *Path, Attempt, *Response.Describe()),
			[this, Verb, Path, Body, OnAnswer = MoveTemp(OnAnswer)]() mutable { Call(Verb, Path, Body, MoveTemp(OnAnswer)); });
	};
	Send(Verb, Path, Body, MoveTemp(OnDone));
}

void FVeyraClientFlow::Probe(EVerb Verb, const FString& Path, FAnswer OnAnswer)
{
	Send(Verb, Path, FString(), [this, WeakAlive = TWeakPtr<bool>(Alive), CallEpoch = Epoch, OnAnswer = MoveTemp(OnAnswer)](const FVeyraBackendResponse& Response) {
		if (!WeakAlive.IsValid() || CallEpoch != Epoch)
		{
			return;
		}
		if (Response.IsUnauthorized())
		{
			EndSession();
			return;
		}
		OnAnswer(Response);
	});
}

void FVeyraClientFlow::Send(EVerb Verb, const FString& Path, const FString& Body, FVeyraBackendCallback OnDone)
{
	switch (Verb)
	{
	case EVerb::Get:
		Backend.Get(Path, GameSession, MoveTemp(OnDone));
		break;
	case EVerb::Post:
		Backend.Post(Path, GameSession, Body, MoveTemp(OnDone));
		break;
	case EVerb::Put:
		Backend.Put(Path, GameSession, Body, MoveTemp(OnDone));
		break;
	case EVerb::Delete:
		Backend.Delete(Path, GameSession, MoveTemp(OnDone));
		break;
	}
}

void FVeyraClientFlow::ShowProblem(const FString& Code, const FString& Message, TFunction<void()> RetryStep)
{
	Snapshot.Problem = FVeyraClientProblem{ Code, Message, static_cast<bool>(RetryStep) };
	RetryAction = MoveTemp(RetryStep);
	UE_LOG(LogVeyraServices, Warning, TEXT("VeyraClientFlow: problem in %s (%s): %s."), LexToString(Snapshot.State), *Code,
		*VeyraBackendProtocol::RedactCredentials(Message));
	Broadcast();
}

void FVeyraClientFlow::ShowRefusal(const FVeyraBackendResponse& Response, const TCHAR* What, TFunction<void()> RetryStep)
{
	ShowProblem(RefusalCode(Response), FString::Printf(TEXT("the backend refused %s: %s"), What, *Response.Describe()), MoveTemp(RetryStep));
}

void FVeyraClientFlow::ShowBadAnswer(const TCHAR* What, const FString& Problem, TFunction<void()> RetryStep)
{
	ShowProblem(TEXT("bad_answer"), FString::Printf(TEXT("the backend's answer about %s was not understood: %s"), What, *Problem), MoveTemp(RetryStep));
}

bool FVeyraClientFlow::LoadHistory(const VeyraBackendProtocol::FHistoryFilter& Filter)
{
	if (!CanIssue(EVeyraClientIntent::LoadHistory))
	{
		return false;
	}
	Log(TEXT("reading the match history."));
	SetBusy(true);
	Call(EVerb::Get, VeyraBackendProtocol::HistoryPath(Filter, FString()), FString(), [this, Filter](const FVeyraBackendResponse& Response) {
		SetBusy(false);
		VeyraBackendProtocol::FHistoryPage Page;
		FString Problem;
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("the match history"), [this, Filter] { LoadHistory(Filter); });
		}
		else if (!VeyraBackendProtocol::ParseHistoryPage(Response.Body, Page, Problem))
		{
			ShowBadAnswer(TEXT("the match history"), Problem, [this, Filter] { LoadHistory(Filter); });
		}
		else
		{
			FVeyraMatchHistory& History = Snapshot.History;
			History.Filter = Filter;
			History.Entries = MoveTemp(Page.Entries);
			History.Next = MoveTemp(Page.Next);
			History.Modes = MoveTemp(Page.Modes);
			History.bLoaded = true;
			History.Opened.Reset();
			Broadcast();
		}
	});
	return true;
}

bool FVeyraClientFlow::LoadMoreHistory()
{
	if (!CanIssue(EVeyraClientIntent::LoadMoreHistory))
	{
		return false;
	}
	Log(TEXT("reading more of the match history."));
	SetBusy(true);
	const VeyraBackendProtocol::FHistoryFilter Filter = Snapshot.History.Filter;
	Call(EVerb::Get, VeyraBackendProtocol::HistoryPath(Filter, Snapshot.History.Next), FString(), [this, Filter](const FVeyraBackendResponse& Response) {
		SetBusy(false);
		VeyraBackendProtocol::FHistoryPage Page;
		FString Problem;
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("more of the match history"), [this] { LoadMoreHistory(); });
		}
		else if (!VeyraBackendProtocol::ParseHistoryPage(Response.Body, Page, Problem))
		{
			ShowBadAnswer(TEXT("more of the match history"), Problem, [this] { LoadMoreHistory(); });
		}
		else if (Snapshot.History.Filter == Filter)
		{
			Snapshot.History.Entries.Append(MoveTemp(Page.Entries));
			Snapshot.History.Next = MoveTemp(Page.Next);
			Snapshot.History.Modes = MoveTemp(Page.Modes);
			Broadcast();
		}
	});
	return true;
}

bool FVeyraClientFlow::OpenHistoryMatch(const FString& MatchId)
{
	const bool bListed = Snapshot.History.Entries.ContainsByPredicate([&MatchId](const VeyraBackendProtocol::FHistoryEntry& Entry) { return Entry.MatchId == MatchId; });
	if (!CanIssue(EVeyraClientIntent::OpenHistoryMatch) || !bListed)
	{
		return false;
	}
	Log(FString::Printf(TEXT("opening match %s from the history."), *MatchId));
	SetBusy(true);
	Call(EVerb::Get, MatchOutcomePath(MatchId), FString(), [this, MatchId](const FVeyraBackendResponse& Response) {
		SetBusy(false);
		VeyraBackendProtocol::FMatchOutcome Outcome;
		FString Problem;
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("the match"), [this, MatchId] { OpenHistoryMatch(MatchId); });
		}
		else if (!VeyraBackendProtocol::ParseMatchOutcome(Response.Body, Outcome, Problem))
		{
			ShowBadAnswer(TEXT("the match"), Problem, [this, MatchId] { OpenHistoryMatch(MatchId); });
		}
		else
		{
			Snapshot.History.Opened = MoveTemp(Outcome);
			Broadcast();
		}
	});
	return true;
}

bool FVeyraClientFlow::CloseHistoryMatch()
{
	if (!CanIssue(EVeyraClientIntent::CloseHistoryMatch))
	{
		return false;
	}
	Snapshot.History.Opened.Reset();
	Broadcast();
	return true;
}

void FVeyraClientFlow::SetBusy(bool bBusy)
{
	if (Snapshot.bBusy != bBusy)
	{
		Snapshot.bBusy = bBusy;
		Broadcast();
	}
}

void FVeyraClientFlow::Broadcast()
{
	++Snapshot.Revision;
	Changed.Broadcast();
}

void FVeyraClientFlow::Log(const FString& Message) const
{
	UE_LOG(LogVeyraServices, Display, TEXT("VeyraClientFlow: %s"), *VeyraBackendProtocol::RedactCredentials(Message));
}
