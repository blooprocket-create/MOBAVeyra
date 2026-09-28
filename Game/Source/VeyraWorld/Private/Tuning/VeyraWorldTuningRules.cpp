// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Layout/VeyraLayout.h"
#include "Tuning/VeyraWorldTuning.h"

namespace VeyraWorld
{
namespace
{
	bool IsOnFloor(const FVeyraMapPoint& Point, double HalfExtent)
	{
		return FMath::Abs(Point.X) <= HalfExtent && FMath::Abs(Point.Y) <= HalfExtent;
	}
}

TArray<FString> Validate(const FVeyraWorldTuning& Tuning)
{
	TArray<FString> Problems;
	const FVeyraBattlegroundLayout& Layout = Tuning.Layout;

	// Canon's three Fluxways, each once (Battleground Bible §2).
	for (const EVeyraLane Lane : { EVeyraLane::Top, EVeyraLane::Mid, EVeyraLane::Bottom })
	{
		const int32 Count = Layout.Lanes.FilterByPredicate([Lane](const FVeyraLaneLayout& Entry) { return Entry.Lane == Lane; }).Num();
		if (Count != 1)
		{
			Problems.Add(FString::Printf(TEXT("/layout/lanes: needs exactly one %s lane, not %d"), *UEnum::GetValueAsString(Lane), Count));
		}
	}

	for (int32 Index = 0; Index < Layout.Lanes.Num(); ++Index)
	{
		const FVeyraLaneLayout& Lane = Layout.Lanes[Index];
		const FString Pointer = FString::Printf(TEXT("/layout/lanes/%d"), Index);
		if (Lane.Points.Num() < 2)
		{
			Problems.Add(Pointer + TEXT("/points: a lane needs at least two points"));
			continue;
		}
		for (const FVeyraMapPoint& Point : Lane.Points)
		{
			if (!IsOnFloor(Point, Layout.HalfExtent))
			{
				Problems.Add(Pointer + TEXT("/points: every point must lie on the floor"));
				break;
			}
		}
		if (!VeyraLayout::MirrorsOntoItself(Lane))
		{
			Problems.Add(Pointer + TEXT("/points: the lane must be its own mirror across Y = -X, reversed, so both teams walk the same distances"));
		}
		double Previous = 0.0;
		for (const double Distance : Lane.SpireDistances)
		{
			if (Distance <= Previous)
			{
				Problems.Add(Pointer + TEXT("/spireDistances: must be above 0 and strictly rising"));
				break;
			}
			Previous = Distance;
		}
		if (Lane.FluxbornSpawnDistance >= VeyraLayout::Length(Lane.Points) / 2.0)
		{
			Problems.Add(Pointer + TEXT("/fluxbornSpawnDistance: Team A's Fluxborn must spawn on its half of the lane"));
		}
		// The widest Fluxborn must fit between the lane's structures where they spawn, or it starts inside one.
		double WidestFluxborn = 0.0;
		for (const TPair<FVeyraContentId, FVeyraFluxbornDefinition>& Unit : Tuning.Fluxborn.Units)
		{
			WidestFluxborn = FMath::Max(WidestFluxborn, Unit.Value.CapsuleRadius);
		}
		const bool bClearOfInhibitor = FMath::Abs(Lane.FluxbornSpawnDistance - Lane.InhibitorDistance) > Tuning.Structures.Inhibitor.CapsuleRadius + WidestFluxborn;
		bool bClearOfSpires = true;
		for (const double Distance : Lane.SpireDistances)
		{
			bClearOfSpires &= FMath::Abs(Lane.FluxbornSpawnDistance - (Lane.InhibitorDistance + Distance)) > Tuning.Structures.LaneSpire.CapsuleRadius + WidestFluxborn;
		}
		if (!bClearOfInhibitor || !bClearOfSpires)
		{
			Problems.Add(Pointer + TEXT("/fluxbornSpawnDistance: Fluxborn would spawn inside the lane's inhibitor or a Spire"));
		}
		const double Outermost = Lane.InhibitorDistance + (Lane.SpireDistances.IsEmpty() ? 0.0 : Lane.SpireDistances.Last());
		if (Outermost >= VeyraLayout::Length(Lane.Points) / 2.0)
		{
			Problems.Add(Pointer + TEXT("/spireDistances: Team A's structures must stay on its half of the lane"));
		}
	}

	const FVeyraBaseLayout& Base = Layout.Base;
	TArray<FVeyraMapPoint> BasePoints = Base.BaseTowers;
	BasePoints.Add(Base.PrimeWell);
	BasePoints.Add(Base.Fountain);
	for (const FVeyraMapPoint& Point : BasePoints)
	{
		if (!IsOnFloor(Point, Layout.HalfExtent))
		{
			Problems.Add(TEXT("/layout/base: every point must lie on the floor"));
			break;
		}
	}
	if (Base.BaseTowers.IsEmpty())
	{
		Problems.Add(TEXT("/layout/base/baseTowers: the Prime Well needs its base-defense towers (Battleground Bible §18)"));
	}

	// Every Fluxborn attacks with the profile Vanguards use, so the same rules check it.
	if (Tuning.Fluxborn.Units.IsEmpty())
	{
		Problems.Add(TEXT("/fluxborn/units: needs at least one kind of Fluxborn"));
	}
	for (const TPair<FVeyraContentId, FVeyraFluxbornDefinition>& Unit : Tuning.Fluxborn.Units)
	{
		const FString Pointer = TEXT("/fluxborn/units/") + Unit.Key.ToString();
		for (const FString& AttackProblem : VeyraBasicAttacks::Validate(Unit.Value.BasicAttack))
		{
			Problems.Add(Pointer + TEXT("/basicAttack: ") + AttackProblem);
		}
		if (Unit.Value.CapsuleHalfHeight < Unit.Value.CapsuleRadius)
		{
			Problems.Add(Pointer + TEXT("/capsuleHalfHeight: must be at least capsuleRadius"));
		}
	}

	// Waves: phases in order from 0, and every unit a kind of Fluxborn (Battleground Bible §17).
	const FVeyraWavesTuning& Waves = Tuning.Waves;
	if (Waves.Phases.IsEmpty() || Waves.Phases[0].FromSeconds != 0.0)
	{
		Problems.Add(TEXT("/waves/phases: the first phase must begin at 0"));
	}
	for (int32 Index = 1; Index < Waves.Phases.Num(); ++Index)
	{
		if (Waves.Phases[Index].FromSeconds <= Waves.Phases[Index - 1].FromSeconds)
		{
			Problems.Add(FString::Printf(TEXT("/waves/phases/%d/fromSeconds: phases must begin in order"), Index));
		}
	}
	const auto CheckUnits = [&Problems, &Tuning](const TArray<FVeyraWaveUnitTuning>& Units, const TCHAR* Field) {
		for (int32 Index = 0; Index < Units.Num(); ++Index)
		{
			if (!Tuning.FindFluxborn(Units[Index].Unit))
			{
				Problems.Add(FString::Printf(TEXT("/waves/%s/%d/unit: %s is no kind of Fluxborn"), Field, Index, *Units[Index].Unit.ToString()));
			}
		}
	};
	CheckUnits(Waves.Units, TEXT("units"));
	CheckUnits(Waves.SiegeUnits, TEXT("siegeUnits"));
	CheckUnits(Waves.InhibitorDownUnits, TEXT("inhibitorDownUnits"));
	return Problems;
}
}
