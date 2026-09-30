// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Battleground/VeyraBattlegroundTypes.h"
#include "Containers/ArrayView.h"
#include "Math/Vector2D.h"
#include "Misc/Optional.h"
#include "Teams/VeyraTeam.h"

struct FVeyraBattlegroundLayout;
struct FVeyraLaneLayout;
struct FVeyraMapPoint;

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
 * The battleground's geometry as pure functions of its layout (ADR-011 §12), so the map commandlet,
 * the server's spawning and the tests agree. Team A's half is authored; Team B's is its mirror.
 */
namespace VeyraLayout
{
	VEYRAWORLD_API FVector2D ToVector(const FVeyraMapPoint& Point);

	/** Team A's point as Team B's: reflected across the river's diagonal, Y = -X. */
	VEYRAWORLD_API FVector2D Mirror(const FVector2D& Point);

	/** The length of the path through Points. */
	VEYRAWORLD_API double Length(TConstArrayView<FVeyraMapPoint> Points);

	/** How far Point lies from the path through Points: from its nearest segment. */
	VEYRAWORLD_API double DistanceToPath(TConstArrayView<FVeyraMapPoint> Points, const FVector2D& Point);

	/**
	 * How far Point lies from the river's diagonal, Y = -X, toward Team A's base: positive on Team A's
	 * half, negative on Team B's.
	 */
	VEYRAWORLD_API double DepthInTeamAHalf(const FVeyraBattlegroundLayout& Layout, const FVector2D& Point);

	/** The point Distance along the path through Points, clamped to its ends. */
	VEYRAWORLD_API FVector2D PointAlong(TConstArrayView<FVeyraMapPoint> Points, double Distance);

	/** The lane's path as Team walks it toward the enemy base: forward for Team A, reversed for Team B. */
	VEYRAWORLD_API TArray<FVector2D> Waypoints(const FVeyraLaneLayout& Lane, EVeyraTeam Team);

	/** Where Team's Vanguards start and respawn. */
	VEYRAWORLD_API FVector2D Fountain(const FVeyraBattlegroundLayout& Layout, EVeyraTeam Team);

	/** A point of Team A's base, as it stands for Team. */
	VEYRAWORLD_API FVector2D ForTeam(const FVector2D& TeamAPoint, EVeyraTeam Team);

	/** Both teams' Dense Fog: Team A's circles, then their mirrors. */
	VEYRAWORLD_API TArray<FVeyraFogPlacement> DenseFog(const FVeyraBattlegroundLayout& Layout);

	/** Every structure of both teams: each lane's Spires and inhibitor, the base towers and the Prime Well. */
	VEYRAWORLD_API TArray<FVeyraStructurePlacement> Structures(const FVeyraBattlegroundLayout& Layout);

	/**
	 * Whether Point is jungle terrain (ADR-026 §5): on the floor, and on no lane's road, the river or
	 * either base's pad, each as wide as the layout draws it, so a new layout needs no extra authoring.
	 */
	VEYRAWORLD_API bool IsJungle(const FVeyraBattlegroundLayout& Layout, const FVector2D& Point);

	/** Whether the lane's path is its own mirror, reversed, so both teams walk the same distances. */
	VEYRAWORLD_API bool MirrorsOntoItself(const FVeyraLaneLayout& Lane);
}
