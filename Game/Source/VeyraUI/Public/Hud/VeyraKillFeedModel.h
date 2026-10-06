// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Feedback/VeyraKillFeedTypes.h"

/** A kill feed line as this client received it, stamped by its own clock. */
struct FVeyraKillFeedArrival
{
	FVeyraKillFeedLine Line;
	double ReceivedAt = 0.0;
};

/** A kill feed row to draw now. */
struct FVeyraKillFeedRow
{
	FVeyraKillFeedLine Line;
	/** How far through its showing it is: 0 as it arrives, nearing 1 as it goes. */
	double Progress = 0.0;
};

/** The announcement over the battleground of a moment that is the player's own, or everyone's (ADR-065 §10). */
struct FVeyraAnnouncement
{
	FString Text;
	/** Good news for the player's side, which shows in its colour; bad news in the enemy's. */
	bool bGood = true;
	double Progress = 0.0;
};

/** The client's kill feed and its announcements, apart from the engine (ADR-065 §10). */
namespace VeyraKillFeedView
{
	/** Drops the arrivals older than KeepSeconds by Now. */
	VEYRAUI_API void Forget(TArray<FVeyraKillFeedArrival>& Arrivals, double Now, double KeepSeconds);

	/** The rows to show at Now: the latest MaxRows that arrived within ShowSeconds, oldest first. */
	VEYRAUI_API TArray<FVeyraKillFeedRow> Rows(TConstArrayView<FVeyraKillFeedArrival> Arrivals, double Now, double ShowSeconds, int32 MaxRows);

	/**
	 * What Line announces to the player OwnPlayerId on OwnSide; empty for none: the player's own takedown and death, First
	 * Blood whoever drew it, and a structure of either side falling.
	 */
	VEYRAUI_API FString AnnouncementOf(const FVeyraKillFeedLine& Line, int32 OwnPlayerId, EVeyraTeam OwnSide);

	/** The latest announcement among the arrivals within Seconds of Now, for the player OwnPlayerId on OwnSide. */
	VEYRAUI_API TOptional<FVeyraAnnouncement> Announcement(TConstArrayView<FVeyraKillFeedArrival> Arrivals, double Now, double Seconds, int32 OwnPlayerId,
		EVeyraTeam OwnSide);

	/** A structure line's name for what fell, such as "top outer Spire" or "Prime Well". */
	VEYRAUI_API FString StructureName(const FVeyraKillFeedLine& Line);
}
