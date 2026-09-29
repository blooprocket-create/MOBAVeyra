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
	/** The mode, in capitals, as League names it in the corner. */
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

	/**
	 * Everything the screens show except the countdown, as text: the shell rebuilds its widgets only
	 * when this changes, so a poll that changes nothing never interrupts a click.
	 */
	VEYRAUI_API FString Signature(const FVeyraClientSnapshot& Snapshot);
}
