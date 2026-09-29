// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Content/VeyraContentId.h"
#include "Internationalization/Text.h"
#include "Teams/VeyraTeam.h"

class APlayerState;

/** One player as the in-match scoreboard shows them (ADR-017 §4, §9.4). */
struct FVeyraScoreboardRow
{
	FString Name;
	FVeyraContentId Vanguard;
	int32 Level = 0;
	int32 Kills = 0;
	int32 Deaths = 0;
	int32 Assists = 0;

	/** Minions and monsters together, as League's creep score counts them. */
	int32 CreepScore = 0;

	/** Each inventory slot's item, in order; invalid for an empty slot. */
	TArray<FVeyraContentId> Items;

	/** Whether this is the viewing player. */
	bool bLocal = false;

	bool operator==(const FVeyraScoreboardRow&) const = default;
};

/** One team's half of the scoreboard. */
struct FVeyraScoreboardSide
{
	EVeyraTeam Team = EVeyraTeam::None;

	/** Whether it is the viewing player's team. */
	bool bAllies = false;

	/** Its players' kills together. */
	int32 Kills = 0;

	/** In seat order. */
	TArray<FVeyraScoreboardRow> Rows;

	bool operator==(const FVeyraScoreboardSide&) const = default;
};

/** Both teams, the viewing player's first. */
struct FVeyraScoreboardView
{
	TArray<FVeyraScoreboardSide> Sides;

	bool operator==(const FVeyraScoreboardView&) const = default;
};

/**
 * The in-match scoreboard's content (ADR-017 §4) from what every client receives: each participant's
 * Vanguard, level, public score and items. It computes no statistic; it reads what the server sent.
 */
namespace VeyraScoreboardModel
{
	/**
	 * Both sides from Participants, the game state's players, with Local's team first; without a
	 * side of its own, Team A leads. Players on no side, such as a replay's spectator, are left out.
	 */
	VEYRAUI_API FVeyraScoreboardView Describe(TConstArrayView<const APlayerState*> Participants, const APlayerState* Local);

	/** K/D/A as League writes it: "3 / 1 / 2". */
	VEYRAUI_API FText KdaText(const FVeyraScoreboardRow& Row);
}
