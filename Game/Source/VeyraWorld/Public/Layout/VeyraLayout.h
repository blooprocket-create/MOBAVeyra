// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Battleground/VeyraBattlegroundTypes.h"
#include "Layout/VeyraWidthCurve.h"
#include "Containers/ArrayView.h"
#include "Math/Vector2D.h"
#include "Misc/Optional.h"
#include "Teams/VeyraTeam.h"
#include "Terrain/VeyraTerrainBox.h"

struct FVeyraBattlegroundLayout;
struct FVeyraLaneLayout;
struct FVeyraMapPoint;
struct FVeyraWallLayout;

/** One Dense Fog circle of either team's half (Battleground Bible §11). */
struct FVeyraFogPlacement
{
	FVector2D Center = FVector2D::ZeroVector;
	double Radius = 0.0;
};

/** Where one structure stands (Battleground Bible §5, §10, §18). */
struct FVeyraStructurePlacement
{
	EVeyraStructureKind Kind = EVeyraStructureKind::LaneSpire;
	EVeyraTeam Team = EVeyraTeam::None;

	/** The lane of a Spire or inhibitor; none for the base's structures. */
	TOptional<EVeyraLane> Lane;

	/**
	 * For a lane's structures, the order they must fall in (author ruling 2026-09-28, Battleground
	 * §10): 0 for the outer Spire, rising inward, the inhibitor last. For the base's structures, the
	 * index among their kind.
	 */
	int32 Order = 0;

	/** On the floor, in world units. */
	FVector2D Location = FVector2D::ZeroVector;
};

/**
 * One wall as it stands for one team (ADR-043 §1, as amended 2026-10-05): its spine sampled, the ridge the terrain raises
 * within it, and the boxes that stand for it wherever something must meet it.
 */
struct VEYRAWORLD_API FVeyraWallShape
{
	EVeyraTeam Team = EVeyraTeam::None;

	/** Its entry in the layout's walls. */
	int32 Index = INDEX_NONE;

	TArray<FVeyraCurveSample> Spine;

	/** How deep inside the wall Point lies, from its nearest edge: negative outside. Its ends are rounded. */
	double DepthInside(const FVector2D& Point) const;

	/**
	 * The boxes that block and hide for it (ADR-043 §2, §3): one to each span of its spine, as thick as the span's thicker
	 * end, mitred where the spine turns so the outside of each bend stays closed, and squared past each end so its
	 * rounded tip stands inside.
	 */
	TArray<FVeyraTerrainBox> Boxes() const;
};

/**
 * The battleground's geometry as pure functions of its layout (ADR-011 §12), so the map commandlet,
 * the server's spawning and the tests agree. Team A's half is authored; Team B's is its rotation half a turn about the
 * centre (author ruling 2026-10-05).
 */
namespace VeyraLayout
{
	VEYRAWORLD_API FVector2D ToVector(const FVeyraMapPoint& Point);

	/** Team A's point as Team B's: turned half a turn about the centre. Directions turn the same way. */
	VEYRAWORLD_API FVector2D Rotate(const FVector2D& Point);

	/** The length of the path through Points. */
	VEYRAWORLD_API double Length(TConstArrayView<FVeyraMapPoint> Points);

	/** How far Point lies from the path through Points: from its nearest segment. */
	VEYRAWORLD_API double DistanceToPath(TConstArrayView<FVeyraMapPoint> Points, const FVector2D& Point);

	/**
	 * How far Point lies from the line through the centre between the bases, X + Y = 0, toward Team A's base: positive
	 * on Team A's half, negative on Team B's. The halves are each other's rotation.
	 */
	VEYRAWORLD_API double DepthInTeamAHalf(const FVeyraBattlegroundLayout& Layout, const FVector2D& Point);

	/** The point Distance along the path through Points, clamped to its ends. */
	VEYRAWORLD_API FVector2D PointAlong(TConstArrayView<FVeyraMapPoint> Points, double Distance);

	/** The lane's path as Team walks it toward the enemy base: forward for Team A, reversed for Team B. */
	VEYRAWORLD_API TArray<FVector2D> Waypoints(const FVeyraLaneLayout& Lane, EVeyraTeam Team);

	/**
	 * Where Team's Fluxborn of Lane spawn: the lane's spawn distance along it from Team's own end, in front of the
	 * team's inhibitor, as its structures stand. Never Team A's spawn rotated: the rotation carries the top lane onto
	 * the bottom.
	 */
	VEYRAWORLD_API FVector2D FluxbornSpawnPoint(const FVeyraLaneLayout& Lane, EVeyraTeam Team);

	/** Where Team's Vanguards start and respawn. */
	VEYRAWORLD_API FVector2D Fountain(const FVeyraBattlegroundLayout& Layout, EVeyraTeam Team);

	/** A point of Team A's base, as it stands for Team. */
	VEYRAWORLD_API FVector2D ForTeam(const FVector2D& TeamAPoint, EVeyraTeam Team);

	/** Both teams' Dense Fog: Team A's circles, then their rotations. */
	VEYRAWORLD_API TArray<FVeyraFogPlacement> DenseFog(const FVeyraBattlegroundLayout& Layout);

	/** Team A's wall Index in the layout's walls, as it stands for Team (ADR-043 §1): Team B's is its rotation. */
	VEYRAWORLD_API FVeyraWallShape WallShape(const FVeyraBattlegroundLayout& Layout, int32 Index, EVeyraTeam Team);

	/** Both teams' walls: Team A's, then their rotations. */
	VEYRAWORLD_API TArray<FVeyraWallShape> WallShapes(const FVeyraBattlegroundLayout& Layout);

	/** Every box of both teams' walls (FVeyraWallShape::Boxes): Team A's walls' first, each wall's in order along it. */
	VEYRAWORLD_API TArray<FVeyraTerrainBox> Walls(const FVeyraBattlegroundLayout& Layout);

	/**
	 * Every structure of both teams: each lane's Spires and inhibitor, the same distances along the lane from each team's
	 * own end, then the base towers and the Prime Well.
	 */
	VEYRAWORLD_API TArray<FVeyraStructurePlacement> Structures(const FVeyraBattlegroundLayout& Layout);

	/**
	 * Whether Point is jungle terrain (ADR-026 §5): on the floor, and on no lane's road, the river's water or either
	 * base's pad, each as wide as the layout draws it, so a new layout needs no extra authoring. A Well's island is ground.
	 */
	VEYRAWORLD_API bool IsJungle(const FVeyraBattlegroundLayout& Layout, const FVector2D& Point);

	/**
	 * Whether Lane, rotated and reversed, is one of Lanes: top onto bottom, mid onto itself. Then each team walks the same
	 * lanes from its own side.
	 */
	VEYRAWORLD_API bool RotatesOntoALane(const FVeyraLaneLayout& Lane, TConstArrayView<FVeyraLaneLayout> Lanes);
}
