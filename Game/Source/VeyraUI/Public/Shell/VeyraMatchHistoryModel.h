// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Client/VeyraClientFlowTypes.h"
#include "Internationalization/Text.h"
#include "Shell/VeyraShellModels.h"

/** One choice of a Match History filter; an empty Value is "all" (UX-64). */
struct FVeyraHistoryOption
{
	FString Value;
	FText Label;
	bool bSelected = false;
};

/** One completed match in the list (UX-51). */
struct FVeyraHistoryRow
{
	FString MatchId;
	/** Its date, mode, duration, the player's Vanguard and outcome, on one line. */
	FText Summary;
};

/** Match History's page (Pre-Game Client UX Bible 51, 64, 67): the filters, the list, or an opened record. */
struct FVeyraHistoryModel
{
	TArray<FVeyraHistoryOption> Vanguards;
	TArray<FVeyraHistoryOption> Modes;
	TArray<FVeyraHistoryOption> Outcomes;
	TArray<FVeyraHistoryRow> Rows;
	/** Said while the list is empty: still loading, or no match fits the filters. */
	FText Empty;
	bool bOffersLoadMore = false;
	/** The opened record, the results screen's view of it, when one is open. */
	TOptional<FVeyraResultsModel> Opened;
};

namespace VeyraMatchHistoryModel
{
	/** The player's outcome as history names it: Victory, Defeat or No Contest. */
	VEYRAUI_API FText OutcomeText(const FString& Outcome);

	VEYRAUI_API FVeyraHistoryModel Describe(const FVeyraClientSnapshot& Snapshot, bool bCanLoadMore);
}
