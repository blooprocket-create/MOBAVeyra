// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Internationalization/Text.h"
#include "Misc/Optional.h"
#include "VeyraMatchTypes.h"

/** What the match loading screen says the client is doing (SET-114; ADR-053 §3). */
enum class EVeyraLoadingStage : uint8
{
	/** The match's world and the player's own Vanguard are still arriving. */
	LoadingMatch,
	/** The player is in; the match waits for its other players. */
	WaitingForPlayers,
};

/** Which entries the loading screen rotates through (SET-118). */
enum class EVeyraLoadingContent : uint8
{
	Both,
	TipsOnly,
	LoreOnly,
	Off,
};

/** One gameplay tip or lore fact. */
struct FVeyraLoadingEntry
{
	FText Text;
	bool bLore = false;
};

/** How long an automatically shown entry stays (SET-120): at least MinimumSeconds, and a second more for each CharactersPerSecond past BaseCharacters. */
struct FVeyraLoadingTiming
{
	double MinimumSeconds = 0.0;
	int32 BaseCharacters = 0;
	int32 CharactersPerSecond = 1;
};

/** One loading screen's rotation through its entries (SET-117, SET-119). */
struct FVeyraLoadingRotation
{
	/** The entries' indices, in this screen's shuffled order. */
	TArray<int32> Order;
	/** Where in Order the shown entry is. */
	int32 Position = 0;
	/** When the shown entry began to show, on the screen's clock. */
	double ShownAt = 0.0;
	/** The player browsed with Previous or Next, which stops automatic rotation for the rest of the screen. */
	bool bManual = false;

	/** The shown entry's index, or none without entries. */
	TOptional<int32> Shown() const { return Order.IsValidIndex(Position) ? TOptional<int32>(Order[Position]) : TOptional<int32>(); }
};

/** The match loading screen's rules, apart from the engine (ADR-053 §3). */
namespace VeyraLoadingModel
{
	/**
	 * The stage to show, or none once loading is done: Loading Match until the match's state and the player's own Vanguard
	 * arrive, then Waiting for Players while the match is still in its Loading phase.
	 */
	VEYRAUI_API TOptional<EVeyraLoadingStage> StageOf(TOptional<EVeyraMatchPhase> Phase, bool bHasOwnVanguard);

	/** The content setting's option as the screen reads it; Both for anything else. */
	VEYRAUI_API EVeyraLoadingContent ParseContent(const FString& Option);

	/** The entries Content enables: tips, lore, both or none. */
	VEYRAUI_API TArray<FVeyraLoadingEntry> EntriesFor(EVeyraLoadingContent Content, TConstArrayView<FText> Tips, TConstArrayView<FText> Lore);

	/** A rotation through Count entries in an order Seed shuffles, so the starting entry varies from match to match, the first showing from Now. */
	VEYRAUI_API FVeyraLoadingRotation Start(int32 Count, int32 Seed, double Now);

	/** How long Text stays when shown automatically. */
	VEYRAUI_API double SecondsFor(const FText& Text, const FVeyraLoadingTiming& Timing);

	/**
	 * Moves on to the next entry once the shown one has had its time, unless the player browsed. Each entry shows once before
	 * any shows again. Returns whether the shown entry changed.
	 */
	VEYRAUI_API bool Advance(FVeyraLoadingRotation& Rotation, TConstArrayView<FVeyraLoadingEntry> Entries, const FVeyraLoadingTiming& Timing, double Now);

	/** Previous (-1) or Next (+1), at once and wrapping around; automatic rotation stops for the rest of the screen. */
	VEYRAUI_API void Browse(FVeyraLoadingRotation& Rotation, int32 Step, double Now);
}
