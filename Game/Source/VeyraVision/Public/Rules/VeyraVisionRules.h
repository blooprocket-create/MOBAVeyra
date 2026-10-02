// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Shapes/VeyraShapes.h"
#include "Math/Vector2D.h"
#include "Targeting/VeyraVisibility.h"
#include "Teams/VeyraTeam.h"
#include "Terrain/VeyraTerrainBox.h"

/** Something that gives its team vision around it (Vision Bible §1). */
struct FVeyraSightSource
{
	EVeyraTeam Team = EVeyraTeam::None;
	FVector2D Position = FVector2D::ZeroVector;
	/** How far it sees, in units; above 0. */
	double Radius = 0.0;

	/** Whether it detects Camouflage: a Vanguard or a standing structure, never a ward (ADR-018 §4). */
	bool bDetects = false;

	/** A lit shape, which sees exactly what lies inside it; unset for a circle of Radius (ADR-018 §5). */
	TOptional<FVeyraPlacedShape> Shape;

	/** A lit area lights what lies inside it directly, so walls hide nothing from it (ADR-043 §3). */
	bool bThroughWalls = false;
};

/** A Dense Fog circle (Vision Bible §2): the battleground's bush, authored on the map or made by an ability. */
struct FVeyraFogCircle
{
	FVector2D Center = FVector2D::ZeroVector;
	/** In units; above 0. */
	double Radius = 0.0;
};

/**
 * The map walls sight is tested against (ADR-043 §3), with a grid that offers each sight line only the
 * walls near it. Built once from the layout. A cell is as large as the largest wall's bounds, so a wall
 * lies in at most four cells and a line of sight crosses only a few.
 */
struct VEYRAVISION_API FVeyraSightWalls
{
	FVeyraSightWalls() = default;
	explicit FVeyraSightWalls(TArray<FVeyraTerrainBox> InWalls);

	/** Whether a wall stands between From and To. */
	bool Blocks(const FVector2D& From, const FVector2D& To) const;

	/** The walls the grid offers the line from From to To, each once: every wall that could cross it. */
	TArray<int32> CandidatesFor(const FVector2D& From, const FVector2D& To) const;

	int32 Num() const { return Walls.Num(); }

private:
	FIntPoint CellOf(const FVector2D& Point) const;

	TArray<FVeyraTerrainBox> Walls;
	double CellSize = 0.0;
	/** Each cell the walls' bounds touch, with the walls touching it. */
	TMap<FIntPoint, TArray<int32>> Cells;
};

/**
 * A square grid over the battleground on which a side's seen ground is published (ADR-054 §2): CellsAcross cells a
 * side, each CellSize wide, from the corner Min.
 */
struct FVeyraSeenGrid
{
	FVector2D Min = FVector2D::ZeroVector;
	double CellSize = 0.0;
	int32 CellsAcross = 0;

	bool IsValid() const { return CellsAcross > 0 && CellSize > 0.0; }

	int32 NumCells() const { return CellsAcross * CellsAcross; }

	FVector2D CentreOf(int32 X, int32 Y) const { return Min + FVector2D((X + 0.5) * CellSize, (Y + 0.5) * CellSize); }

	/** The grid of CellsAcross cells a side over the square reaching HalfExtent from Centre. */
	static FVeyraSeenGrid Over(const FVector2D& Centre, double HalfExtent, int32 CellsAcross)
	{
		FVeyraSeenGrid Grid;
		Grid.CellsAcross = CellsAcross;
		Grid.CellSize = CellsAcross > 0 ? 2.0 * HalfExtent / CellsAcross : 0.0;
		Grid.Min = Centre - FVector2D(HalfExtent, HalfExtent);
		return Grid;
	}
};

/** Vision's rules, as plain functions of positions (ADR-016 §2). */
namespace VeyraVisionRules
{
	/** Whether one of Walls stands between From and To, so neither sees the other (ADR-043 §3). */
	VEYRAVISION_API bool IsBlocked(const FVeyraSightWalls& Walls, const FVector2D& From, const FVector2D& To);

	/**
	 * Whether one of Team's Sources has Point within its sight: its circle, or its shape, with none of
	 * Walls between them unless it lights through them.
	 */
	VEYRAVISION_API bool IsSeenBy(EVeyraTeam Team, TConstArrayView<FVeyraSightSource> Sources, const FVector2D& Point,
		const FVeyraSightWalls& Walls = FVeyraSightWalls());

	/**
	 * The cells of Grid whose centres Team's Sources see, by IsSeenBy's rules (ADR-054 §2), packed eight to a byte:
	 * cell (X, Y) is bit (Y * CellsAcross + X) % 8 of byte (Y * CellsAcross + X) / 8. Each source tests only the cells
	 * within its reach. Dense Fog darkens nothing: the fog itself is always seen (Vision Bible §2).
	 */
	VEYRAVISION_API TArray<uint8> SeenCells(EVeyraTeam Team, TConstArrayView<FVeyraSightSource> Sources, const FVeyraSeenGrid& Grid,
		const FVeyraSightWalls& Walls = FVeyraSightWalls());

	/** Whether cell (X, Y) of Grid is seen in Cells, as SeenCells packs them; false outside the grid. */
	VEYRAVISION_API bool IsCellSeen(TConstArrayView<uint8> Cells, const FVeyraSeenGrid& Grid, int32 X, int32 Y);

	/**
	 * Whether one of Team's detecting Sources has Point within both its sight and DetectionRadius, with
	 * none of Walls between them: how a Camouflaged unit is seen (Combat Bible §11; ADR-018 §4).
	 */
	VEYRAVISION_API bool IsDetectedBy(EVeyraTeam Team, TConstArrayView<FVeyraSightSource> Sources, const FVector2D& Point, double DetectionRadius,
		const FVeyraSightWalls& Walls = FVeyraSightWalls());

	/**
	 * The fog volumes: circles that overlap or touch are one volume while they do (Vision Bible §2).
	 * For each circle, its volume's number, from 0, in the order volumes first appear.
	 */
	VEYRAVISION_API TArray<int32> ConnectVolumes(TConstArrayView<FVeyraFogCircle> Circles);

	/** The volume holding Point, as numbered by ConnectVolumes (Volumes), or INDEX_NONE outside every circle. */
	VEYRAVISION_API int32 VolumeAt(TConstArrayView<FVeyraFogCircle> Circles, TConstArrayView<int32> Volumes, const FVector2D& Point);

	/** The first of Circles holding Point, or INDEX_NONE: the circle a presence ping names (ADR-016 §5). */
	VEYRAVISION_API int32 CircleAt(TConstArrayView<FVeyraFogCircle> Circles, const FVector2D& Point);

	/**
	 * The circles Dense Fog of Shape is made of (ADR-036 §1): a circle is itself; a corridor is circles of
	 * half its width, the first and last within its ends, each the next's radius apart so they overlap.
	 */
	VEYRAVISION_API TArray<FVeyraFogCircle> CirclesOf(const FVeyraFogShape& Shape);
}
