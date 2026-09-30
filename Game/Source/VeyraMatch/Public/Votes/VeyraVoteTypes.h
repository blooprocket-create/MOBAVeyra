// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Teams/VeyraTeam.h"

#include "VeyraVoteTypes.generated.h"

/** What a vote decides (Match Flow Bible §7–§10; ADR-019 §4). */
UENUM()
enum class EVeyraVoteKind : uint8
{
	/** One team's vote to end the match as no contest, early on. */
	Remake,
	/** One team's vote to lose now. */
	Surrender,
	/** Everyone's vote to stop play for an intermission. */
	Pause,
	/** Everyone's vote to end an intermission early. */
	Resume,
};

/** Why a vote may not start, or a ballot may not count. */
UENUM()
enum class EVeyraVoteRefusal : uint8
{
	None,
	/** Only a standard match takes votes: a practice match's host ends it. */
	NotStandard,
	/** Votes need the live match. */
	NotLive,
	/** A surrender before its time. */
	TooEarly,
	/** A remake after its time. */
	TooLate,
	/** One vote at a time. */
	AnotherVote,
	/** A failed vote of this kind waits its cooldown. */
	CoolingDown,
	/** An early resume needs a pause, and a pause needs play. */
	NotPaused,
	AlreadyPaused,
	/** Not on the voting team, or not a player. */
	NotAVoter,
	/** A recorded ballot is locked. */
	AlreadyVoted,
	/** No vote is open. */
	NoVote,
};

/** The vote the match holds, as every player sees it (ADR-019 §4). */
USTRUCT()
struct FVeyraVoteState
{
	GENERATED_BODY()

	UPROPERTY()
	bool bOpen = false;

	UPROPERTY()
	EVeyraVoteKind Kind = EVeyraVoteKind::Remake;

	/** The team that votes; None when everyone does. */
	UPROPERTY()
	EVeyraTeam Team = EVeyraTeam::None;

	UPROPERTY()
	int32 Yes = 0;

	UPROPERTY()
	int32 No = 0;

	/** The YES votes that pass it. */
	UPROPERTY()
	int32 Needed = 0;

	/** Real seconds until it closes. */
	UPROPERTY()
	float SecondsLeft = 0.0f;

	/** The PlayerIds that have a ballot recorded, chosen or automatic. */
	UPROPERTY()
	TArray<int32> Voted;

	bool operator==(const FVeyraVoteState& Other) const = default;
};
