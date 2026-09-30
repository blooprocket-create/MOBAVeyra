// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Statistics/VeyraMatchStatistics.h"

namespace VeyraStatisticsRules
{
double UnionSeconds(TArray<FVeyraSpan> Spans)
{
	Spans.RemoveAll([](const FVeyraSpan& Span) { return !(Span.End > Span.Start); });
	Spans.Sort([](const FVeyraSpan& A, const FVeyraSpan& B) { return A.Start < B.Start; });
	double Total = 0.0;
	double CoveredUntil = -UE_BIG_NUMBER;
	for (const FVeyraSpan& Span : Spans)
	{
		// Only what reaches past the time already counted is new.
		const double From = FMath::Max(Span.Start, CoveredUntil);
		if (Span.End > From)
		{
			Total += Span.End - From;
			CoveredUntil = Span.End;
		}
	}
	return Total;
}

bool AddEarned(FVeyraGoldBySource& Sources, EVeyraGoldReason Reason, double Amount)
{
	double* Source = nullptr;
	switch (Reason)
	{
	case EVeyraGoldReason::Starting:
		Source = &Sources.Starting;
		break;
	case EVeyraGoldReason::Kill:
	case EVeyraGoldReason::FirstBlood:
	// A claimed bounty is Gold from a kill (ADR-020 §3).
	case EVeyraGoldReason::Bounty:
		Source = &Sources.Kills;
		break;
	case EVeyraGoldReason::Assist:
		Source = &Sources.Assists;
		break;
	case EVeyraGoldReason::LastHit:
	case EVeyraGoldReason::Participation:
		Source = &Sources.Minions;
		break;
	case EVeyraGoldReason::Wildlife:
		Source = &Sources.Jungle;
		break;
	case EVeyraGoldReason::StructurePool:
	case EVeyraGoldReason::FirstStructure:
	case EVeyraGoldReason::FluxWell:
		Source = &Sources.Objectives;
		break;
	case EVeyraGoldReason::WardDestroyed:
		Source = &Sources.Wards;
		break;
	case EVeyraGoldReason::Passive:
		Source = &Sources.Passive;
		break;
	// Gold given back or given by a developer was never earned (ADR-017 §9).
	case EVeyraGoldReason::Sale:
	case EVeyraGoldReason::Undo:
	case EVeyraGoldReason::Developer:
		break;
	}
	if (!Source || !FMath::IsFinite(Amount) || !(Amount > 0.0))
	{
		return false;
	}
	*Source += Amount;
	return true;
}
}
