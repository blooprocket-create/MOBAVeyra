// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Misc/Optional.h"
#include "Votes/VeyraVoteTypes.h"

struct FVeyraVotesTuning;

/** One player who may take part in a vote, as the match sees them now. */
struct FVeyraVoter
{
	int32 PlayerId = 0;
	EVeyraTeam Team = EVeyraTeam::None;
	bool bBot = false;
	bool bDisconnected = false;
	bool bAfk = false;
};

/** An open vote and its ballots. Times are real seconds. */
struct FVeyraBallotBox
{
	EVeyraVoteKind Kind = EVeyraVoteKind::Remake;
	/** The team that votes; None when everyone does. */
	EVeyraTeam Team = EVeyraTeam::None;
	double EndsAt = 0.0;
	/** Each recorded ballot by PlayerId: YES or NO, locked once recorded. */
	TMap<int32, bool> Ballots;
};

/** When each kind of vote may start again after one failed, in real seconds. */
struct FVeyraVoteCooldowns
{
	TMap<EVeyraTeam, double> RemakeUntil;
	TMap<EVeyraTeam, double> SurrenderUntil;
	double PauseUntil = 0.0;
};

/** How a vote stands. */
enum class EVeyraVoteOutcome : uint8
{
	Open,
	Passed,
	Failed,
};

/** The pure rules of remake, surrender and pause votes (Match Flow Bible §7–§10; ADR-019 §4). */
namespace VeyraVotes
{
	/** Whether a vote of Kind may start now, for a player of Team: None if it may. */
	struct FStartContext
	{
		EVeyraVoteKind Kind = EVeyraVoteKind::Remake;
		EVeyraTeam Team = EVeyraTeam::None;
		/** Match seconds on the clock, which a pause stops. */
		double MatchClock = 0.0;
		/** Real seconds now. */
		double Now = 0.0;
		bool bStandard = false;
		bool bLive = false;
		bool bPaused = false;
		bool bVoteOpen = false;
	};
	VEYRAMATCH_API EVeyraVoteRefusal CheckStart(const FStartContext& Context, const FVeyraVoteCooldowns& Cooldowns, const FVeyraVotesTuning& Tuning);

	/** Whether the vote is for everyone rather than one team. */
	VEYRAMATCH_API bool IsUnanimous(EVeyraVoteKind Kind);

	/** Whether Voter takes part: its team's players in a team vote, everyone in a unanimous one. */
	VEYRAMATCH_API bool IsVoter(const FVeyraBallotBox& Box, const FVeyraVoter& Voter);

	/**
	 * The ballot a voter casts without choosing, if any (§9; ADR-019 §9): a disconnected teammate
	 * votes YES on a remake and abstains on a surrender; a bot abstains on its team's vote; in a
	 * unanimous vote, a disconnected, AFK or bot player votes YES.
	 */
	VEYRAMATCH_API TOptional<bool> AutomaticBallot(EVeyraVoteKind Kind, const FVeyraVoter& Voter);

	/** The YES votes that pass it: the tuned majority of a full team, or every voter. */
	VEYRAMATCH_API int32 YesNeeded(const FVeyraBallotBox& Box, TConstArrayView<FVeyraVoter> Voters, const FVeyraVotesTuning& Tuning);

	/**
	 * How the vote stands at Now: passed on enough YES; failed once its window closes, on any NO in a
	 * unanimous vote, or once the YES votes and the players who could still vote fall short.
	 */
	VEYRAMATCH_API EVeyraVoteOutcome Tally(const FVeyraBallotBox& Box, TConstArrayView<FVeyraVoter> Voters, double Now, const FVeyraVotesTuning& Tuning);

	/** The window a vote of Kind stays open for, in real seconds. */
	VEYRAMATCH_API double WindowSeconds(EVeyraVoteKind Kind, const FVeyraVotesTuning& Tuning);

	/** Starts the cooldown a failed vote leaves behind. */
	VEYRAMATCH_API void NoteFailed(const FVeyraBallotBox& Box, double Now, const FVeyraVotesTuning& Tuning, FVeyraVoteCooldowns& Cooldowns);
}
