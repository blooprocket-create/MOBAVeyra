// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Rules/VeyraVisionRules.h"

namespace VeyraVisionRules
{
bool IsSeenBy(EVeyraTeam Team, TConstArrayView<FVeyraSightSource> Sources, const FVector2D& Point)
{
	for (const FVeyraSightSource& Source : Sources)
	{
		if (Source.Team == Team && FVector2D::DistSquared(Source.Position, Point) <= FMath::Square(Source.Radius))
		{
			return true;
		}
	}
	return false;
}

TArray<int32> ConnectVolumes(TConstArrayView<FVeyraFogCircle> Circles)
{
	// Union-find over the pairs that touch; a map holds a handful of circles.
	TArray<int32> Parent;
	for (int32 Index = 0; Index < Circles.Num(); ++Index)
	{
		Parent.Add(Index);
	}
	const auto Root = [&Parent](int32 Index) {
		while (Parent[Index] != Index)
		{
			Parent[Index] = Parent[Parent[Index]];
			Index = Parent[Index];
		}
		return Index;
	};
	for (int32 First = 0; First < Circles.Num(); ++First)
	{
		for (int32 Second = First + 1; Second < Circles.Num(); ++Second)
		{
			const double Reach = Circles[First].Radius + Circles[Second].Radius;
			if (FVector2D::DistSquared(Circles[First].Center, Circles[Second].Center) <= FMath::Square(Reach))
			{
				Parent[Root(First)] = Root(Second);
			}
		}
	}
	TMap<int32, int32> Numbers;
	TArray<int32> Volumes;
	for (int32 Index = 0; Index < Circles.Num(); ++Index)
	{
		const int32 Next = Numbers.Num();
		Volumes.Add(Numbers.FindOrAdd(Root(Index), Next));
	}
	return Volumes;
}

int32 VolumeAt(TConstArrayView<FVeyraFogCircle> Circles, TConstArrayView<int32> Volumes, const FVector2D& Point)
{
	for (int32 Index = 0; Index < Circles.Num() && Index < Volumes.Num(); ++Index)
	{
		if (FVector2D::DistSquared(Circles[Index].Center, Point) <= FMath::Square(Circles[Index].Radius))
		{
			return Volumes[Index];
		}
	}
	return INDEX_NONE;
}
}
