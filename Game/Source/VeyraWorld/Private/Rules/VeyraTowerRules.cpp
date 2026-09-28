// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Rules/VeyraTowerRules.h"

namespace VeyraTowerRules
{
namespace
{
	const FVeyraTowerCandidate* Find(TConstArrayView<FVeyraTowerCandidate> InRange, const AActor* Unit)
	{
		return Unit ? InRange.FindByPredicate([Unit](const FVeyraTowerCandidate& Candidate) { return Candidate.Unit == Unit; }) : nullptr;
	}

	const FVeyraTowerCandidate* Nearest(TConstArrayView<FVeyraTowerCandidate> InRange, bool bVanguards)
	{
		const FVeyraTowerCandidate* Best = nullptr;
		for (const FVeyraTowerCandidate& Candidate : InRange)
		{
			if (Candidate.bVanguard != bVanguards)
			{
				continue;
			}
			if (!Best || Candidate.Distance < Best->Distance || (Candidate.Distance == Best->Distance && Candidate.StableId < Best->StableId))
			{
				Best = &Candidate;
			}
		}
		return Best;
	}
}

FVeyraTowerChoice Choose(const AActor* Current, bool bCurrentPriority, const AActor* Claimant, TConstArrayView<FVeyraTowerCandidate> InRange)
{
	const bool bCurrentValid = Find(InRange, Current) != nullptr;
	if (bCurrentPriority && bCurrentValid)
	{
		return { Current, true };
	}
	if (const FVeyraTowerCandidate* Claim = Find(InRange, Claimant); Claim && Claim->bVanguard)
	{
		return { Claimant, true };
	}
	if (bCurrentValid)
	{
		return { Current, false };
	}
	// Normally towers shoot hostile Fluxborn; with none, a Vanguard.
	const FVeyraTowerCandidate* Next = Nearest(InRange, /*bVanguards*/ false);
	Next = Next ? Next : Nearest(InRange, /*bVanguards*/ true);
	return { Next ? Next->Unit : nullptr, false };
}

int32 NextRampStacks(const AActor* LastTarget, int32 LastStacks, const AActor* Target, bool bTargetIsVanguard, int32 MaxStacks)
{
	if (!bTargetIsVanguard || !Target || Target != LastTarget)
	{
		return 0;
	}
	return FMath::Min(LastStacks + 1, MaxStacks);
}

double RampMultiplier(int32 Stacks, double PerShot)
{
	return 1.0 + PerShot * FMath::Max(0, Stacks);
}
}
