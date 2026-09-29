// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Votes/VeyraVoteRules.h"

#include "Tuning/VeyraMatchTuning.h"

namespace VeyraVotes
{
EVeyraVoteRefusal CheckStart(const FStartContext& Context, const FVeyraVoteCooldowns& Cooldowns, const FVeyraVotesTuning& Tuning)
{
	if (!Context.bStandard)
	{
		return EVeyraVoteRefusal::NotStandard;
	}
	if (!Context.bLive)
	{
		return EVeyraVoteRefusal::NotLive;
	}
	if (Context.bVoteOpen)
	{
		return EVeyraVoteRefusal::AnotherVote;
	}
	if (Context.Team == EVeyraTeam::None)
	{
		return EVeyraVoteRefusal::NotAVoter;
	}
	switch (Context.Kind)
	{
	case EVeyraVoteKind::Remake:
		if (Context.bPaused)
		{
			return EVeyraVoteRefusal::AlreadyPaused;
		}
		// The cutoff is for starting it; one started in time may finish after (§7).
		if (Context.MatchClock > Tuning.Remake.StartBeforeSeconds)
		{
			return EVeyraVoteRefusal::TooLate;
		}
		if (Context.Now < Cooldowns.RemakeUntil.FindRef(Context.Team))
		{
			return EVeyraVoteRefusal::CoolingDown;
		}
		return EVeyraVoteRefusal::None;
	case EVeyraVoteKind::Surrender:
		if (Context.bPaused)
		{
			return EVeyraVoteRefusal::AlreadyPaused;
		}
		if (Context.MatchClock < Tuning.Surrender.StartAfterSeconds)
		{
			return EVeyraVoteRefusal::TooEarly;
		}
		if (Context.Now < Cooldowns.SurrenderUntil.FindRef(Context.Team))
		{
			return EVeyraVoteRefusal::CoolingDown;
		}
		return EVeyraVoteRefusal::None;
	case EVeyraVoteKind::Pause:
		if (Context.bPaused)
		{
			return EVeyraVoteRefusal::AlreadyPaused;
		}
		return Context.Now < Cooldowns.PauseUntil ? EVeyraVoteRefusal::CoolingDown : EVeyraVoteRefusal::None;
	case EVeyraVoteKind::Resume:
		return Context.bPaused ? EVeyraVoteRefusal::None : EVeyraVoteRefusal::NotPaused;
	}
	return EVeyraVoteRefusal::NoVote;
}

bool IsUnanimous(EVeyraVoteKind Kind)
{
	return Kind == EVeyraVoteKind::Pause || Kind == EVeyraVoteKind::Resume;
}

bool IsVoter(const FVeyraBallotBox& Box, const FVeyraVoter& Voter)
{
	return IsUnanimous(Box.Kind) ? Voter.Team != EVeyraTeam::None : Voter.Team == Box.Team;
}

TOptional<bool> AutomaticBallot(EVeyraVoteKind Kind, const FVeyraVoter& Voter)
{
	if (IsUnanimous(Kind))
	{
		return Voter.bBot || Voter.bDisconnected || Voter.bAfk ? TOptional<bool>(true) : TOptional<bool>();
	}
	if (Voter.bBot)
	{
		return {};
	}
	return Kind == EVeyraVoteKind::Remake && Voter.bDisconnected ? TOptional<bool>(true) : TOptional<bool>();
}

int32 YesNeeded(const FVeyraBallotBox& Box, TConstArrayView<FVeyraVoter> Voters, const FVeyraVotesTuning& Tuning)
{
	switch (Box.Kind)
	{
	case EVeyraVoteKind::Remake:
		return Tuning.Remake.YesVotes;
	case EVeyraVoteKind::Surrender:
		return Tuning.Surrender.YesVotes;
	case EVeyraVoteKind::Pause:
	case EVeyraVoteKind::Resume:
		break;
	}
	int32 Everyone = 0;
	for (const FVeyraVoter& Voter : Voters)
	{
		Everyone += IsVoter(Box, Voter) ? 1 : 0;
	}
	return Everyone;
}

EVeyraVoteOutcome Tally(const FVeyraBallotBox& Box, TConstArrayView<FVeyraVoter> Voters, double Now, const FVeyraVotesTuning& Tuning)
{
	int32 Yes = 0;
	int32 No = 0;
	int32 Undecided = 0;
	for (const FVeyraVoter& Voter : Voters)
	{
		if (!IsVoter(Box, Voter))
		{
			continue;
		}
		if (const bool* Ballot = Box.Ballots.Find(Voter.PlayerId))
		{
			(*Ballot ? Yes : No) += 1;
		}
		// Only a player who is there can still choose: an absent one's ballot is automatic or none.
		else if (!Voter.bBot && !Voter.bDisconnected)
		{
			Undecided += 1;
		}
	}
	const int32 Needed = YesNeeded(Box, Voters, Tuning);
	if (Needed > 0 && Yes >= Needed)
	{
		return EVeyraVoteOutcome::Passed;
	}
	if (Now >= Box.EndsAt || Yes + Undecided < Needed || (IsUnanimous(Box.Kind) && No > 0) || Needed <= 0)
	{
		return EVeyraVoteOutcome::Failed;
	}
	return EVeyraVoteOutcome::Open;
}

double ClosesAt(EVeyraVoteKind Kind, double Now, const TOptional<double>& IntermissionEndsAt, const FVeyraVotesTuning& Tuning)
{
	switch (Kind)
	{
	case EVeyraVoteKind::Remake:
		return Now + Tuning.Remake.WindowSeconds;
	case EVeyraVoteKind::Surrender:
		return Now + Tuning.Surrender.WindowSeconds;
	case EVeyraVoteKind::Resume:
		// Its ballots hold while the intermission lasts; when that ends, play resumes anyway.
		if (IntermissionEndsAt.IsSet())
		{
			return FMath::Max(Now, IntermissionEndsAt.GetValue());
		}
		break;
	case EVeyraVoteKind::Pause:
		break;
	}
	return Now + Tuning.Pause.WindowSeconds;
}

void NoteFailed(const FVeyraBallotBox& Box, double Now, const FVeyraVotesTuning& Tuning, FVeyraVoteCooldowns& Cooldowns)
{
	switch (Box.Kind)
	{
	case EVeyraVoteKind::Remake:
		Cooldowns.RemakeUntil.Add(Box.Team, Now + Tuning.Remake.CooldownSeconds);
		break;
	case EVeyraVoteKind::Surrender:
		Cooldowns.SurrenderUntil.Add(Box.Team, Now + Tuning.Surrender.CooldownSeconds);
		break;
	case EVeyraVoteKind::Pause:
		Cooldowns.PauseUntil = Now + Tuning.Pause.CooldownSeconds;
		break;
	case EVeyraVoteKind::Resume:
		// A failed early resume costs nothing: the intermission ends by itself (§10.3).
		break;
	}
}
}
