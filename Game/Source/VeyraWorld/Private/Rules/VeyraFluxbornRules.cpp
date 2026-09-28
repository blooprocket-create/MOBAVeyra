// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Rules/VeyraFluxbornRules.h"

namespace VeyraFluxbornRules
{
namespace
{
	/** Lower ranks come first (ADR-011 §7). */
	enum class ERank : uint8
	{
		Responding,
		SiegeStructure,
		Fluxborn,
		Structure,
		Vanguard,
	};

	ERank RankOf(const FVeyraFluxbornCandidate& Candidate, bool bSiege)
	{
		switch (Candidate.Kind)
		{
		case EVeyraUnitKind::Structure:
			return bSiege && Candidate.bInAttackRange ? ERank::SiegeStructure : ERank::Structure;
		case EVeyraUnitKind::Fluxborn:
			return ERank::Fluxborn;
		case EVeyraUnitKind::Vanguard:
		// Fluxborn never choose neutral units (ADR-014 §1); were one offered, it would come last.
		case EVeyraUnitKind::Wildlife:
		case EVeyraUnitKind::Objective:
			return ERank::Vanguard;
		}
		return ERank::Vanguard;
	}

	bool IsBetter(const FVeyraFluxbornCandidate& Candidate, ERank Rank, const FVeyraFluxbornCandidate* Best, ERank BestRank)
	{
		if (!Best || Rank != BestRank)
		{
			return !Best || Rank < BestRank;
		}
		return Candidate.Distance < Best->Distance || (Candidate.Distance == Best->Distance && Candidate.StableId < Best->StableId);
	}
}

FVeyraFluxbornChoice Choose(const AActor* Current, bool bResponding, const AActor* Claimant, bool bSiege, TConstArrayView<FVeyraFluxbornCandidate> InRange)
{
	const FVeyraFluxbornCandidate* CurrentCandidate = Current ? InRange.FindByPredicate([Current](const FVeyraFluxbornCandidate& Each) { return Each.Unit == Current; }) : nullptr;
	if (CurrentCandidate && bResponding)
	{
		return { Current, true };
	}
	const FVeyraFluxbornCandidate* Claim = Claimant ? InRange.FindByPredicate([Claimant](const FVeyraFluxbornCandidate& Each) { return Each.Unit == Claimant; }) : nullptr;
	if (Claim && Claim->Kind == EVeyraUnitKind::Vanguard)
	{
		return { Claimant, true };
	}

	const FVeyraFluxbornCandidate* Best = nullptr;
	ERank BestRank = ERank::Vanguard;
	for (const FVeyraFluxbornCandidate& Candidate : InRange)
	{
		const ERank Rank = RankOf(Candidate, bSiege);
		if (IsBetter(Candidate, Rank, Best, BestRank))
		{
			Best = &Candidate;
			BestRank = Rank;
		}
	}
	// A valid current target is kept against others of its own rank.
	if (CurrentCandidate && RankOf(*CurrentCandidate, bSiege) <= BestRank)
	{
		return { Current, false };
	}
	return { Best ? Best->Unit : nullptr, false };
}

namespace
{
	/** The segment of the path nearest Location: its index, how far along it (0 to 1), and the distance to it. */
	struct FNearestSegment
	{
		int32 Index = 0;
		double Along = 0.0;
		double Distance = 0.0;
	};

	FNearestSegment NearestSegment(TConstArrayView<FVector2D> Waypoints, const FVector2D& Location)
	{
		FNearestSegment Nearest;
		Nearest.Distance = TNumericLimits<double>::Max();
		for (int32 Index = 0; Index + 1 < Waypoints.Num(); ++Index)
		{
			const FVector2D& From = Waypoints[Index];
			const FVector2D Segment = Waypoints[Index + 1] - From;
			const double LengthSquared = Segment.SizeSquared();
			const double Along = LengthSquared > 0.0 ? FMath::Clamp(FVector2D::DotProduct(Location - From, Segment) / LengthSquared, 0.0, 1.0) : 0.0;
			const double Distance = FVector2D::Distance(Location, From + Segment * Along);
			if (Distance < Nearest.Distance)
			{
				Nearest = { Index, Along, Distance };
			}
		}
		return Nearest;
	}
}

int32 ResumeWaypoint(TConstArrayView<FVector2D> Waypoints, const FVector2D& Location)
{
	if (Waypoints.Num() < 2)
	{
		return 0;
	}
	const FNearestSegment Nearest = NearestSegment(Waypoints, Location);
	// At the very end of a segment, the next one's end is ahead.
	return Nearest.Along >= 1.0 ? FMath::Min(Nearest.Index + 2, Waypoints.Num() - 1) : Nearest.Index + 1;
}

double DistanceFromLane(TConstArrayView<FVector2D> Waypoints, const FVector2D& Location)
{
	if (Waypoints.Num() < 2)
	{
		return Waypoints.IsEmpty() ? 0.0 : FVector2D::Distance(Location, Waypoints[0]);
	}
	return NearestSegment(Waypoints, Location).Distance;
}
}
