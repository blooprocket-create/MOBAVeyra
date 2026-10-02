// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Rules/VeyraVisionRules.h"

FVeyraSightWalls::FVeyraSightWalls(TArray<FVeyraTerrainBox> InWalls)
	: Walls(MoveTemp(InWalls))
{
	for (const FVeyraTerrainBox& Wall : Walls)
	{
		CellSize = FMath::Max(CellSize, Wall.Bounds().GetSize().GetMax());
	}
	if (!(CellSize > 0.0))
	{
		return;
	}
	for (int32 Index = 0; Index < Walls.Num(); ++Index)
	{
		const FBox2D Bounds = Walls[Index].Bounds();
		const FIntPoint Low = CellOf(Bounds.Min);
		const FIntPoint High = CellOf(Bounds.Max);
		for (int32 X = Low.X; X <= High.X; ++X)
		{
			for (int32 Y = Low.Y; Y <= High.Y; ++Y)
			{
				Cells.FindOrAdd(FIntPoint(X, Y)).Add(Index);
			}
		}
	}
}

FIntPoint FVeyraSightWalls::CellOf(const FVector2D& Point) const
{
	return FIntPoint(FMath::FloorToInt32(Point.X / CellSize), FMath::FloorToInt32(Point.Y / CellSize));
}

TArray<int32> FVeyraSightWalls::CandidatesFor(const FVector2D& From, const FVector2D& To) const
{
	TArray<int32> Candidates;
	if (Cells.IsEmpty())
	{
		return Candidates;
	}
	// The cells under the line's bounds: a wall that crosses the line lies in one of them.
	const FIntPoint Low = CellOf(FVector2D(FMath::Min(From.X, To.X), FMath::Min(From.Y, To.Y)));
	const FIntPoint High = CellOf(FVector2D(FMath::Max(From.X, To.X), FMath::Max(From.Y, To.Y)));
	for (int32 X = Low.X; X <= High.X; ++X)
	{
		for (int32 Y = Low.Y; Y <= High.Y; ++Y)
		{
			if (const TArray<int32>* InCell = Cells.Find(FIntPoint(X, Y)))
			{
				for (const int32 Index : *InCell)
				{
					Candidates.AddUnique(Index);
				}
			}
		}
	}
	return Candidates;
}

bool FVeyraSightWalls::Blocks(const FVector2D& From, const FVector2D& To) const
{
	if (Cells.IsEmpty())
	{
		return false;
	}
	// As CandidatesFor, without gathering: a wall in two of the cells is only tested twice.
	const FIntPoint Low = CellOf(FVector2D(FMath::Min(From.X, To.X), FMath::Min(From.Y, To.Y)));
	const FIntPoint High = CellOf(FVector2D(FMath::Max(From.X, To.X), FMath::Max(From.Y, To.Y)));
	for (int32 X = Low.X; X <= High.X; ++X)
	{
		for (int32 Y = Low.Y; Y <= High.Y; ++Y)
		{
			const TArray<int32>* InCell = Cells.Find(FIntPoint(X, Y));
			if (InCell && InCell->ContainsByPredicate([this, &From, &To](int32 Index) { return Walls[Index].Crosses(From, To); }))
			{
				return true;
			}
		}
	}
	return false;
}

namespace VeyraVisionRules
{
bool IsBlocked(const FVeyraSightWalls& Walls, const FVector2D& From, const FVector2D& To)
{
	return Walls.Blocks(From, To);
}

namespace
{
	/** Whether Source has Point within its sight: its circle or shape, with no wall between unless it lights through. */
	bool Sees(const FVeyraSightSource& Source, const FVector2D& Point, const FVeyraSightWalls& Walls)
	{
		const bool bInside = Source.Shape.IsSet() ? VeyraShapes::Touches(Source.Shape.GetValue(), FVector(Point, 0.0), 0.0)
												  : FVector2D::DistSquared(Source.Position, Point) <= FMath::Square(Source.Radius);
		// Within its reach first: a wall is looked for only between a source and what it could see.
		return bInside && (Source.bThroughWalls || !IsBlocked(Walls, Source.Position, Point));
	}
}

bool IsSeenBy(EVeyraTeam Team, TConstArrayView<FVeyraSightSource> Sources, const FVector2D& Point, const FVeyraSightWalls& Walls)
{
	for (const FVeyraSightSource& Source : Sources)
	{
		if (Source.Team == Team && Sees(Source, Point, Walls))
		{
			return true;
		}
	}
	return false;
}

TArray<uint8> SeenCells(EVeyraTeam Team, TConstArrayView<FVeyraSightSource> Sources, const FVeyraSeenGrid& Grid, const FVeyraSightWalls& Walls)
{
	TArray<uint8> Cells;
	if (!Grid.IsValid())
	{
		return Cells;
	}
	Cells.SetNumZeroed((Grid.NumCells() + 7) / 8);
	const auto CellAt = [&Grid](double Offset) { return FMath::Clamp(FMath::FloorToInt32(Offset / Grid.CellSize), 0, Grid.CellsAcross - 1); };
	for (const FVeyraSightSource& Source : Sources)
	{
		if (Source.Team != Team || !(Source.Radius > 0.0))
		{
			continue;
		}
		// A shaped source's Radius is its shape's reach from its position, so one square bounds both kinds.
		const int32 LowX = CellAt(Source.Position.X - Source.Radius - Grid.Min.X);
		const int32 HighX = CellAt(Source.Position.X + Source.Radius - Grid.Min.X);
		const int32 LowY = CellAt(Source.Position.Y - Source.Radius - Grid.Min.Y);
		const int32 HighY = CellAt(Source.Position.Y + Source.Radius - Grid.Min.Y);
		for (int32 Y = LowY; Y <= HighY; ++Y)
		{
			for (int32 X = LowX; X <= HighX; ++X)
			{
				const int32 Index = Y * Grid.CellsAcross + X;
				uint8& Byte = Cells[Index / 8];
				const uint8 Bit = static_cast<uint8>(1u << (Index % 8));
				if (!(Byte & Bit) && Sees(Source, Grid.CentreOf(X, Y), Walls))
				{
					Byte |= Bit;
				}
			}
		}
	}
	return Cells;
}

bool IsCellSeen(TConstArrayView<uint8> Cells, const FVeyraSeenGrid& Grid, int32 X, int32 Y)
{
	if (X < 0 || Y < 0 || X >= Grid.CellsAcross || Y >= Grid.CellsAcross)
	{
		return false;
	}
	const int32 Index = Y * Grid.CellsAcross + X;
	return Cells.IsValidIndex(Index / 8) && (Cells[Index / 8] & (1u << (Index % 8))) != 0;
}

bool IsDetectedBy(EVeyraTeam Team, TConstArrayView<FVeyraSightSource> Sources, const FVector2D& Point, double DetectionRadius,
	const FVeyraSightWalls& Walls)
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
