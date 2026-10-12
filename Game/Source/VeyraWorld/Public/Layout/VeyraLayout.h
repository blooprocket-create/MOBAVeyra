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
struct FVeyraMassLayout;
struct FVeyraKeepWallLayout;
enum class EVeyraMassKind : uint8;

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
 * One rock or forest mass (ADR-073 §2): blocked ground, as generated for both teams. Nothing stands inside it, so what
 * blocks and hides is a ring of boxes along its edge (§3).
 */
struct VEYRAWORLD_API FVeyraMassShape
{
	/** Its entry in the layout's masses. */
	int32 Index = INDEX_NONE;

	EVeyraMassKind Kind{};

	/** Its outline, counter-clockwise. */
	TArray<FVector2D> Outline;

	/** How deep inside the mass Point lies, from its nearest edge: negative outside. */
	double DepthInside(const FVector2D& Point) const;

	/**
	 * The boxes along its edge (ADR-073 §3): each Thickness deep just inside one span of its outline, no longer than
	 * MaxLength, so a long span takes several. Every crossing of its edge crosses one. Where the mass is a neck thinner
	 * than Thickness, a box reaches only to the neck's far side, so none stands out of the mass.
	 */
	TArray<FVeyraTerrainBox> EdgeBoxes(double Thickness, double MaxLength) const;

	/** Its outline filled as triangles, three corners each in turn, as a map draws it. */
	TArray<FVector2D> Triangles() const;
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

	/** Every mass of both teams, as generated (ADR-073 §2). */
	VEYRAWORLD_API TArray<FVeyraMassShape> Masses(const FVeyraBattlegroundLayout& Layout);

	/** Team's keep floor (ADR-073 §1): Team A's as authored, Team B's its rotation. */
	VEYRAWORLD_API TArray<FVector2D> KeepFloor(const FVeyraBattlegroundLayout& Layout, EVeyraTeam Team);

	/** Whether Point is inside either team's keep. */
	VEYRAWORLD_API bool IsInKeep(const FVeyraBattlegroundLayout& Layout, const FVector2D& Point);

	/** The boxes of one keep wall run as it stands for Team: one to each span, as thick as the run, mitred at each bend. */
	VEYRAWORLD_API TArray<FVeyraTerrainBox> KeepWallRun(const FVeyraKeepWallLayout& Run, EVeyraTeam Team);

	/** The boxes of both teams' keep walls: every run's, Team A's then Team B's. */
	VEYRAWORLD_API TArray<FVeyraTerrainBox> KeepWalls(const FVeyraBattlegroundLayout& Layout);

	/**
	 * How far the playable square reaches from the centre: the floor less its rim's band (ADR-073 §2). Every open space
	 * lies inside it, and the masses fill the rest of it.
	 */
	VEYRAWORLD_API double PlayableHalfExtent(const FVeyraBattlegroundLayout& Layout);

	/**
	 * The rim (ADR-073 §3): boxes along each side of the playable square, just outside it and as deep as a mass's edge, so
	 * the band between it and the floor's edge, which no mass covers, is closed wherever an open space meets the square.
	 */
	VEYRAWORLD_API TArray<FVeyraTerrainBox> Rim(const FVeyraBattlegroundLayout& Layout);

	/**
	 * Every box of the battleground's terrain (ADR-073 §3): each mass's edge ring, both keeps' walls, then the rim. What
	 * blocks movement, paths, forced moves, line projectiles and sight (ADR-043 §3).
	 */
	VEYRAWORLD_API TArray<FVeyraTerrainBox> Walls(const FVeyraBattlegroundLayout& Layout);

	/** Whether Point lies in Polygon (its corners in order, either way round). */
	VEYRAWORLD_API bool IsInPolygon(TConstArrayView<FVector2D> Polygon, const FVector2D& Point);

	/** How deep inside Polygon Point lies, from its nearest edge: negative outside. */
	VEYRAWORLD_API double DepthInPolygon(TConstArrayView<FVector2D> Polygon, const FVector2D& Point);

	/**
	 * Every structure of both teams: each lane's Spires and inhibitor, the same distances along the lane from each team's
	 * own end, each Spire stood aside of the road by the layout's SpireOffset on its side; then the base towers and the
	 * Prime Well.
	 */
	VEYRAWORLD_API TArray<FVeyraStructurePlacement> Structures(const FVeyraBattlegroundLayout& Layout);

	/**
	 * Whether Point is jungle terrain (ADR-026 §5): on the floor, and on no lane's road, the river's water or either
	 * keep's floor, each as the layout draws it, so a new layout needs no extra authoring. A Well's island is ground.
	 */
	VEYRAWORLD_API bool IsJungle(const FVeyraBattlegroundLayout& Layout, const FVector2D& Point);

	/**
	 * Whether Lane, rotated and reversed, is one of Lanes: top onto bottom, mid onto itself. Then each team walks the same
	 * lanes from its own side.
	 */
	VEYRAWORLD_API bool RotatesOntoALane(const FVeyraLaneLayout& Lane, TConstArrayView<FVeyraLaneLayout> Lanes);
}
