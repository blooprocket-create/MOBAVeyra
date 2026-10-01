// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Rules/VeyraVisionRules.h"

namespace VeyraVisionRules
{
bool IsBlocked(TConstArrayView<FVeyraTerrainBox> Walls, const FVector2D& From, const FVector2D& To)
{
	return Walls.ContainsByPredicate([&From, &To](const FVeyraTerrainBox& Wall) { return Wall.Crosses(From, To); });
}

bool IsSeenBy(EVeyraTeam Team, TConstArrayView<FVeyraSightSource> Sources, const FVector2D& Point, TConstArrayView<FVeyraTerrainBox> Walls)
{
	for (const FVeyraSightSource& Source : Sources)
	{
		if (Source.Team != Team)
		{
			continue;
		}
		const bool bInside = Source.Shape.IsSet() ? VeyraShapes::Touches(Source.Shape.GetValue(), FVector(Point, 0.0), 0.0)
												  : FVector2D::DistSquared(Source.Position, Point) <= FMath::Square(Source.Radius);
		// Within its reach first: a wall is looked for only between a source and what it could see.
		if (bInside && (Source.bThroughWalls || !IsBlocked(Walls, Source.Position, Point)))
		{
			return true;
		}
	}
	return false;
}

bool IsDetectedBy(EVeyraTeam Team, TConstArrayView<FVeyraSightSource> Sources, const FVector2D& Point, double DetectionRadius,
	TConstArrayView<FVeyraTerrainBox> Walls)
{
	for (const FVeyraSightSource& Source : Sources)
	{
		const double Reach = FMath::Min(Source.Radius, DetectionRadius);
		if (Source.Team == Team && Source.bDetects && FVector2D::DistSquared(Source.Position, Point) <= FMath::Square(Reach)
			&& (Source.bThroughWalls || !IsBlocked(Walls, Source.Position, Point)))
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

int32 CircleAt(TConstArrayView<FVeyraFogCircle> Circles, const FVector2D& Point)
{
	for (int32 Index = 0; Index < Circles.Num(); ++Index)
	{
		if (FVector2D::DistSquared(Circles[Index].Center, Point) <= FMath::Square(Circles[Index].Radius))
		{
			return Index;
		}
	}
	return INDEX_NONE;
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

TArray<FVeyraFogCircle> CirclesOf(const FVeyraFogShape& Shape)
{
	const FVector2D Origin(Shape.Origin);
	if (Shape.Kind == EVeyraFogShapeKind::Circle)
	{
		return { FVeyraFogCircle{ Origin, Shape.Radius } };
	}
	// Circles of half the width, from one radius in from its start to one radius in from its end, spaced by
	// at most a radius so each overlaps the next; a corridor no longer than its width is one circle at its middle.
	const double Radius = Shape.Width / 2.0;
	const FVector2D Along = FVector2D(Shape.Direction).GetSafeNormal();
	const double Span = Shape.Length - 2.0 * Radius;
	if (!(Span > 0.0) || Along.IsNearlyZero())
	{
		return { FVeyraFogCircle{ Origin + Along * (Shape.Length / 2.0), Radius } };
	}
	const int32 Gaps = FMath::CeilToInt32(Span / Radius);
	TArray<FVeyraFogCircle> Circles;
	for (int32 Index = 0; Index <= Gaps; ++Index)
	{
		Circles.Add(FVeyraFogCircle{ Origin + Along * (Radius + Span * Index / Gaps), Radius });
	}
	return Circles;
}
}
