// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Client/VeyraClientFlowTypes.h"
#include "Containers/Array.h"
#include "Internationalization/Text.h"

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
	/** Ordinary pre-game pages: Home and Play (UX §1). */
	Shell,
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
	EVeyraSeatStatus Status = EVeyraSeatStatus::Waiting;
	FText StatusText;
	bool bYou = false;
};

struct FVeyraSelectCardModel
{
	FString VanguardId;
	FText Name;
	/** The player's own hover or lock. */
	bool bChosen = false;
};

/** Champion select as the screen shows it. */
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
};

/** The results screen. */
struct FVeyraResultsModel
{
	/** Whether the backend confirmed how the match ended (UX-15). */
	bool bVerified = false;
	FText Headline;
	TArray<FText> Lines;
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

	VEYRAUI_API FVeyraSelectModel DescribeSelect(const FVeyraClientSnapshot& Snapshot, double RemainingSeconds, bool bCanHover, bool bCanLock);

	VEYRAUI_API FVeyraResultsModel DescribeResults(const FVeyraClientSnapshot& Snapshot);

	/**
	 * Everything the screens show except the countdown, as text: the shell rebuilds its widgets only
	 * when this changes, so a poll that changes nothing never interrupts a click.
	 */
	VEYRAUI_API FString Signature(const FVeyraClientSnapshot& Snapshot);
}
