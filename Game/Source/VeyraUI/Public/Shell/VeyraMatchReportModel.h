// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Backend/VeyraBackendProtocol.h"
#include "Internationalization/Text.h"

/** One player's row on a match's scoreboard (Pre-Game Client UX Bible 50). */
struct FVeyraReportLine
{
	FText Vanguard;
	FText Name;
	bool bYou = false;
	FText Level;
	FText Kda;
	FText Gold;
	/** Minion and jungle last hits, apart (UX-50). */
	FText LastHits;
	/** The final items in slot order, empty slots marked. */
	FText Items;
	/** Both final Flux Spell slots. */
	FText FluxSpells;
};

/** One team's half of the scoreboard with its summary (UX-50, UX-53). */
struct FVeyraReportTeam
{
	FText Title;
	/** Its Vanguard kills, the Gold its players earned, and the Flux Wells it secured, each capture once. */
	FText Summary;
	TArray<FVeyraReportLine> Lines;
};

/** One figure of Detailed Statistics, with its value for each player in column order. */
struct FVeyraReportRow
{
	FText Label;
	TArray<FText> Values;
};

/** A category of Detailed Statistics: Combat, Objectives, Economy or Vision (UX-50). */
struct FVeyraReportGroup
{
	FText Title;
	TArray<FVeyraReportRow> Rows;
};

/**
 * A match's saved Scoreboard and Detailed Statistics (UX-50, UX-53; ADR-017 §6), from the verified
 * result alone: the results screen and Match History show the same report. It computes no statistic;
 * it adds up the team summary and formats what the server recorded.
 */
struct FVeyraMatchReport
{
	/** False while the match has no recorded scoreboard; Pending then says why (UX-50). */
	bool bHasScoreboard = false;
	FText Pending;
	/** Both teams, the viewer's first. */
	TArray<FVeyraReportTeam> Teams;
	/** Detailed Statistics' columns: each player, in the scoreboard's order. */
	TArray<FText> Columns;
	TArray<FVeyraReportGroup> Groups;
};

namespace VeyraMatchReportModel
{
	VEYRAUI_API FVeyraMatchReport Describe(const VeyraBackendProtocol::FMatchOutcome& Outcome);
}
