// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Companions/VeyraCompanionRules.h"

namespace VeyraCompanionRules
{
namespace
{
	/** Lower is fought first; none for a candidate the mode never fights. */
	TOptional<int32> RankOf(EVeyraCompanionMode Mode, const FVeyraCompanionCandidate& Candidate)
	{
		// A summoned companion fights the unit it was bound to, or nothing: never a choice (ADR-035 §5).
		if (Mode == EVeyraCompanionMode::Escort || Mode == EVeyraCompanionMode::Hunt)
		{
			return {};
		}
		if (Candidate.bOwnersTarget)
		{
			return 0;
		}
		if (Mode == EVeyraCompanionMode::Follow)
		{
			return {};
		}
		return Candidate.bVanguard ? 1 : 2;
	}

	bool IsNearer(const FVeyraCompanionCandidate& Candidate, const FVeyraCompanionCandidate& Best)
	{
		return Candidate.Distance < Best.Distance || (Candidate.Distance == Best.Distance && Candidate.StableId < Best.StableId);
	}
}

const AActor* Choose(EVeyraCompanionMode Mode, const AActor* Current, TConstArrayView<FVeyraCompanionCandidate> Candidates)
{
	const FVeyraCompanionCandidate* Best = nullptr;
	int32 BestRank = 0;
	const FVeyraCompanionCandidate* Kept = nullptr;
	int32 KeptRank = 0;
	for (const FVeyraCompanionCandidate& Candidate : Candidates)
	{
		const TOptional<int32> Rank = RankOf(Mode, Candidate);
		if (!Candidate.Unit || !Rank.IsSet())
		{
			continue;
		}
		if (Candidate.Unit == Current)
		{
			Kept = &Candidate;
			KeptRank = Rank.GetValue();
		}
		if (!Best || Rank.GetValue() < BestRank || (Rank.GetValue() == BestRank && IsNearer(Candidate, *Best)))
		{
			Best = &Candidate;
			BestRank = Rank.GetValue();
		}
	}
	// It keeps at its target while nothing of a better rank turns up.
	if (Kept && KeptRank <= BestRank)
	{
		return Kept->Unit;
	}
	return Best ? Best->Unit : nullptr;
}
}
