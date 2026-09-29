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
	return Problems;
}
}
