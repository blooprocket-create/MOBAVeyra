// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Client/VeyraClientFlowTypes.h"
#include "Containers/Array.h"
#include "Internationalization/Text.h"
#include "Shell/VeyraMatchReportModel.h"

/** Which screen the shell shows for a client state (ADR-010 §2, §4). */
enum class EVeyraShellScreen : uint8
{
	/** Nothing: the match's own presentation is on screen. */
	None,
	/** A status and nothing to do but wait, or Retry a problem. */
	Status,
	/** Signing in failed, or the session ended: the reason, and Quit. */
	Stopped,
	/** The one-time starter choice (ADR-010 §6). */
	StarterChoice,
	/** Ordinary pre-game pages: Home and Play (UX §1), and the party panel. */
	Shell,
	/** A custom lobby (ADR-021): both sides' seats, its rules and the friends panel, until its host starts it. */
	Lobby,
	/**
	 * Match Found: Accept or Decline, blocking everything else until answered (UX §5). The grey box
	 * shows it in place of the page, which returns as it was when the match found is over.
	 */
	MatchFound,
	/** Champion select, which owns the whole client (UX-4). */
	ChampionSelect,
	/** Match in Progress, with Reconnect as the only action (UX-17). */
	ReconnectOnly,
	/** The verified result (UX-15). */
	Results,
};

/** A seat's selection status in the team overview (UX-35). */
enum class EVeyraSeatStatus : uint8
{
	Waiting,
	/** A tentative hover. */
	NotLockedIn,
	LockedIn,
};

struct FVeyraSelectSeatModel
{
	FText Name;
	/** The hovered or locked Vanguard; empty when neither is known. */
	FText Vanguard;
	/** Its content ID, for its portrait; empty when neither is known. */
	FString VanguardId;
	/** The seat's starting Flux Spells' names in slot order, empty for an empty slot; only the player's own are known (ADR-015 §5). */
	TArray<FText> Spells;
	EVeyraSeatStatus Status = EVeyraSeatStatus::Waiting;
	FText StatusText;
	bool bYou = false;
	/** On the player's side. */
	bool bAlly = true;
};

struct FVeyraSelectCardModel
{
	FString VanguardId;
	FText Name;
	/** The player's own hover or lock. */
	bool bChosen = false;
	/** Locked by another player: picks are unique in a matchmade select. */
	bool bTaken = false;
};

/** One Flux Spell a slot may take, or none (Pre-Game Client UX Bible 36; ADR-015 §5). */
struct FVeyraSpellChoiceModel
{
	/** Empty for no spell. */
	FString SpellId;
	FText Name;
	FText Description;
	bool bChosen = false;
};

/** One of the two Flux Spell slots in champion select, with what may fill it. */
struct FVeyraSpellSlotModel
{
	/** From 0, in unlock order. */
	int32 Slot = 0;
	/** "Flux Spell 1". */
	FText Title;
	/** "Unlocks at 25 permanent Team Flux": the slot's own threshold (Battleground Bible §14). */
	FText Unlock;
	/** The chosen spell's name, or "Empty". */
	FText Chosen;
	/** The chosen spell's ID; empty for none. */
	FString ChosenId;
	/** None first, then every roster spell. */
	TArray<FVeyraSpellChoiceModel> Choices;
};

/** Champion select as the screen shows it. */
/** One of the shown Vanguard's abilities, as View Abilities lists them. */
struct FVeyraAbilityLineModel
{
	/** "Passive", "Q", "W", "E" or "R". */
	FText Key;
	FText Name;
	FText Description;
	/** The ability's or passive's ID, which its icon is found by. */
	FString AbilityId;
};
struct FVeyraSelectModel
{
	FText Title;
	/** "0:27": the backend's pick timer, rounded up. */
	FText Countdown;
	/** What happens now: picking, locked in, or the match being created. */
	FText Phase;
	TArray<FVeyraSelectSeatModel> Seats;
	TArray<FVeyraSelectCardModel> Cards;
	/** The Vanguard Lock In would lock: the player's hover. Empty for none. */
	FString LockInVanguardId;
	bool bCanChoose = false;
	bool bCanLockIn = false;
	/** Seats on both sides: the overview shows the player's team and the enemy team apart. */
	bool bTeams = false;
	/** A matchmade select still picking offers Leave, which cancels it for everyone (a dodge). */
	bool bOffersLeave = false;
	bool bCanLeave = false;
	/** The player's two starting Flux Spell slots; empty when the player has no seat. */
	TArray<FVeyraSpellSlotModel> SpellSlots;
	/** Choosing never waits for lock-in and never touches the timer (UX 36). */
	bool bCanChooseSpells = false;
	/** "Your Match Setup" once locked in (UX 38): the Vanguard and both spells with their thresholds. Empty before. */
	FText Setup;
	/** The Vanguard the large art shows: the player's lock, else their hover (UX 27). Empty for none. */
	FString ShownVanguardId;
	FText ShownName;
	/** Such as "The River's Grasp". */
	FText ShownTitle;
	/** The shown Vanguard's passive, then Q, W, E and R. */
	TArray<FVeyraAbilityLineModel> Abilities;
	/** The mode, in capitals, as the corner names it. */
	FText ModeLabel;
	/** The pick timer's full length, for its bars; 0 when the backend does not say. */
	double PickSeconds = 0.0;
};

/** A mode card on the Play page (UX-12). */
struct FVeyraModeCardModel
{
	FString ModeId;
	FText Name;
	/** Such as "1v1": the human players on each team. */
	FText Format;
	/** Whether it can be chosen at all: only modes with a matchmaker can. */
	bool bAvailable = false;
	/** For a mode that cannot be chosen: why. */
	FText Availability;
	/** The party's mode. */
	bool bSelected = false;
};

/** The party panel: its mode, roster, readiness and queue (UX §3, UX-2, UX-6). */
struct FVeyraPartyModel
{
	/** False while the player has no party: there is no panel. */
	bool bShown = false;
	FText Mode;
	/** One line per member: name, "(you)", "(leader)", and Ready or Not Ready. */
	TArray<FText> Members;
	/** What happens next, such as who must ready up; empty while queued, when the queue timer shows instead. */
	FText Status;
	bool bQueued = false;
	/** The Ready toggle: what pressing it sets, its label, and whether it can be pressed. */
	bool bReadyTarget = true;
	FText ReadyLabel;
	bool bCanReady = false;
	/** Find Match and Cancel are the leader's (Parties & Social Bible §2). */
	bool bOffersFindMatch = false;
	bool bCanFindMatch = false;
	bool bOffersCancel = false;
	bool bCanCancel = false;
};

/** The Match Found overlay (UX §5). */
struct FVeyraMatchFoundModel
{
	FText Title;
	FText Mode;
	/** The backend's acceptance timer, rounded up. */
	FText Countdown;
	/** Such as "1 of 2 accepted"; nobody learns who. */
	FText Progress;
	/** Accept to play, or waiting for the others once accepted. */
	FText Phase;
	bool bCanAnswer = false;
};

/** The results screen. */
struct FVeyraResultsModel
{
	/** Whether the backend confirmed how the match ended (UX-15). */
	bool bVerified = false;
	FText Headline;
	TArray<FText> Lines;
	/** The Scoreboard and Detailed Statistics, once the result is verified (UX-50). */
	FVeyraMatchReport Report;
};

/** One seat of a custom lobby's screen, and what its host may do there (Custom Matches Bible §1–§3). */
struct FVeyraLobbySeatModel
{
	/** "A" or "B". */
	FString Side;
	int32 Index = 0;
	VeyraBackendProtocol::ELobbySeatKind Kind = VeyraBackendProtocol::ELobbySeatKind::Empty;
	/** A human's name, a bot's Vanguard, or "Empty". */
	FText Name;
	/** "Host", "Beginner Bot" and the like; empty for an empty seat. */
	FText Detail;
	/** A human's account and name, as their buttons name them. */
	FString AccountId;
	FString PlayerName;
	/** A bot's Vanguard, for its portrait, and its difficulty. */
	FString VanguardId;
	FString Difficulty;
	bool bYou = false;
	/** The host's actions here: seat or change a bot, remove it, remove a human, or move one to the other side. */
	bool bCanSetBot = false;
	bool bCanRemoveBot = false;
	bool bCanKick = false;
	bool bCanSwitchSide = false;
	/** Where moving to the other side seats a human: its first empty seat. */
	FString SwitchToSide;
	int32 SwitchToIndex = INDEX_NONE;
};

/** A starting Gold the lobby's host may choose. */
struct FVeyraGoldChoiceModel
{
	/** Unset for the game's own. */
	TOptional<double> Gold;
	FText Label;
	bool bChosen = false;
};

/** A custom lobby as its screen shows it (ADR-021). */
struct FVeyraLobbyModel
{
	/** "DevOne's Lobby". */
	FText Title;
	/** Each side's seats, in order. */
	TArray<FVeyraLobbySeatModel> SideA;
	TArray<FVeyraLobbySeatModel> SideB;
	/** Whether the player hosts it, and so decides everything about it. */
	bool bHost = false;
	/** "Victory: on. Destroying a Prime Well wins." */
	FText Victory;
	bool bVictoryEnabled = false;
	/** Victory needs a Vanguard on each side; the host may switch it only then. */
	bool bCanToggleVictory = false;
	/** "Starting Gold: the game's own." */
	FText StartingGold;
	TArray<FVeyraGoldChoiceModel> GoldChoices;
	bool bCanSetGold = false;
	/** What happens next: who starts it, or that champion select is opening. */
	FText Status;
	bool bCanStart = false;
	bool bCanLeave = false;
};

/** One Vanguard a bot may play, in the bot picker. */
struct FVeyraBotChoiceModel
{
	FString VanguardId;
	FText Name;
	/** Another bot on the same side already plays it. */
	bool bTaken = false;
	/** The seat's bot plays it now. */
	bool bChosen = false;
};

/** The bot picker over the lobby: one seat's Vanguard and difficulty (§2–§3). */
struct FVeyraBotPickerModel
{
	/** "Bot for Side B, Seat 2". */
	FText Title;
	TArray<FVeyraBotChoiceModel> Vanguards;
	/** Each difficulty, easiest first: its ID and name. */
	TArray<TPair<FString, FText>> Difficulties;
};

/** A friend in the friends panel. */
struct FVeyraFriendModel
{
	FString AccountId;
	FText Name;
	/** In the lobby, the host may invite a friend who is not in it yet. */
	bool bOffersInvite = false;
	bool bCanInvite = false;
};

/** A friend request to the player, or an invitation into another player's lobby. */
struct FVeyraSocialRequestModel
{
	/** The requester's account, or the invitation's ID. */
	FString Id;
	FText Name;
	FText Line;
};

/** The friends panel down the right of the shell and the lobby (Parties & Social Bible §1; Art Bible §7). */
struct FVeyraFriendsModel
{
	/** False until the lists have been read once. */
	bool bLoaded = false;
	/** What came of the player's last request, such as "Friend request sent to DevTwo."; empty for none. */
	FText Feedback;
	TArray<FVeyraSocialRequestModel> Invitations;
	bool bCanAnswerInvitations = false;
	bool bCanJoin = false;
	TArray<FVeyraSocialRequestModel> Requests;
	bool bCanAnswerRequests = false;
	TArray<FVeyraFriendModel> Friends;
	/** The players the player asked, who have not answered. */
	TArray<FText> Pending;
	bool bCanAdd = false;
};

/** A title and a line of detail. */
struct FVeyraStatusModel
{
	FText Title;
	FText Detail;
};

/**
 * What the shell shows, from the coordinator's snapshot. Pure functions, so tests check them without
 * widgets. Content IDs are shown title-cased until Vanguards and modes have localized names.
 */
namespace VeyraShellModels
{
	VEYRAUI_API EVeyraShellScreen ScreenFor(EVeyraClientState State);

	/** A content ID as a name: "custom_practice" becomes "Custom Practice". */
	VEYRAUI_API FText NameOf(const FString& ContentId);

	/** A Vanguard's name, from VeyraContentText; NameOf for an ID the table does not know. */
	VEYRAUI_API FText VanguardNameOf(const FString& VanguardId);

	/** The Status and Stopped screens' title and detail. */
	VEYRAUI_API FVeyraStatusModel DescribeStatus(const FVeyraClientSnapshot& Snapshot);

	/** Why the player is here, from the snapshot's notice; empty for none. */
	VEYRAUI_API FText DescribeNotice(const FString& Notice);

	/** A problem as the player reads it. */
	VEYRAUI_API FText DescribeProblem(const FVeyraClientProblem& Problem);

	/** "m:ss", rounded up to the whole second. */
	VEYRAUI_API FText FormatCountdown(double Seconds);

	/** "Flux Spell 1": a Flux Spell slot's name, from 0. */
	VEYRAUI_API FText SpellSlotTitle(int32 Slot);

	VEYRAUI_API FVeyraSelectModel DescribeSelect(const FVeyraClientSnapshot& Snapshot, double RemainingSeconds, bool bCanHover, bool bCanLock, bool bCanLeave,
		bool bCanChooseSpells = false);

	/** The enabled modes, in the backend's order. A mode is shown even when it has no matchmaker yet, as not yet available. */
	VEYRAUI_API TArray<FVeyraModeCardModel> DescribeModes(const FVeyraClientSnapshot& Snapshot);

	/** The party panel; the flags say which of its intents the coordinator allows now. */
	VEYRAUI_API FVeyraPartyModel DescribeParty(const FVeyraClientSnapshot& Snapshot, bool bCanReady, bool bCanFindMatch, bool bCanCancel);

	/** The queue's status: its elapsed time, and no estimate until one can be made honestly (UX-2). */
	VEYRAUI_API FText FormatQueueStatus(double QueuedSeconds);

	VEYRAUI_API FVeyraMatchFoundModel DescribeMatchFound(const FVeyraClientSnapshot& Snapshot, double RemainingSeconds, bool bCanAnswer);

	VEYRAUI_API FVeyraResultsModel DescribeResults(const FVeyraClientSnapshot& Snapshot);

	/** A verified match's headline, lines and report, as the results screen and Match History show it. */
	VEYRAUI_API FVeyraResultsModel DescribeOutcome(const VeyraBackendProtocol::FMatchOutcome& Outcome);

	/**
	 * The lobby's screen. bHosts says the coordinator lets the player change the lobby now, bCanStart and
	 * bCanLeave whether it would start or leave it. With no lobby, an empty model.
	 */
	VEYRAUI_API FVeyraLobbyModel DescribeLobby(const FVeyraClientSnapshot& Snapshot, bool bHosts, bool bCanStart, bool bCanLeave);

	/** The bot picker for the seat Index of Side. */
	VEYRAUI_API FVeyraBotPickerModel DescribeBotPicker(const FVeyraClientSnapshot& Snapshot, const FString& Side, int32 Index);

	/** The friends panel; the flags say which of its intents the coordinator allows now. */
	VEYRAUI_API FVeyraFriendsModel DescribeFriends(const FVeyraClientSnapshot& Snapshot, bool bCanAdd, bool bCanAnswerRequests, bool bCanJoin,
		bool bCanAnswerInvitations, bool bCanInvite);

	/** What came of a social request, from the snapshot's feedback code and the name it was for; empty for none. */
	VEYRAUI_API FText DescribeSocialFeedback(const FString& Code, const FString& Name);

	/** "Side A". */
	VEYRAUI_API FText SideName(const FString& Side);

	/** A bot difficulty's name: "Beginner". */
	VEYRAUI_API FText DifficultyName(const FString& Difficulty);

	// The labels of the lobby's and the friends panel's buttons. Each names its seat or player, so every
	// button on screen is told apart, as a script finding one by its label needs.

	/** "Add Bot: Side B, Seat 2", on an empty seat, and "Change Bot: ..." on a bot's. */
	VEYRAUI_API FText AddBotLabel(const FString& Side, int32 Index);
	VEYRAUI_API FText ChangeBotLabel(const FString& Side, int32 Index);
	VEYRAUI_API FText RemoveBotLabel(const FString& Side, int32 Index);
	VEYRAUI_API FText KickLabel(const FString& Name);
	/** "Move DevTwo to Side B". */
	VEYRAUI_API FText SwitchSideLabel(const FString& Name, const FString& ToSide);
	VEYRAUI_API FText InviteLabel(const FString& Name);
	VEYRAUI_API FText AcceptRequestLabel(const FString& Name);
	VEYRAUI_API FText DeclineRequestLabel(const FString& Name);
	VEYRAUI_API FText JoinLobbyLabel(const FString& Name);
	VEYRAUI_API FText DeclineInviteLabel(const FString& Name);
	/** "Default Gold", or "1,500 Gold". */
	VEYRAUI_API FText StartingGoldLabel(TOptional<double> Gold);
	/** The bot picker's choice of a Vanguard: "Bot: Cairn". */
	VEYRAUI_API FText BotChoiceLabel(const FString& VanguardId);

	/**
	 * Everything the screens show except the countdown, as text: the shell rebuilds its widgets only
	 * when this changes, so a poll that changes nothing never interrupts a click.
	 */
	VEYRAUI_API FString Signature(const FVeyraClientSnapshot& Snapshot);
}
