// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Battleground/VeyraBattlegroundTypes.h"
#include "Containers/ArrayView.h"
#include "Misc/Optional.h"
#include "Teams/VeyraTeam.h"

/** What the structure rules need to know of one structure. */
struct FVeyraStructureStatus
{
	EVeyraStructureKind Kind = EVeyraStructureKind::LaneSpire;
	EVeyraTeam Team = EVeyraTeam::None;
	TOptional<EVeyraLane> Lane;

	/** Its place in its lane's falling order, from 0 for the outer Spire; unused in the base. */
	int32 Order = 0;

	/** Destroyed and not rebuilt. */
	bool bDestroyed = false;
};

/**
 * Which structures may be damaged now, and when the Prime Well regenerates (Battleground Bible §10,
 * §18; lane order ruled 2026-09-28; ADR-011 §9). No world: the caller lists every structure.
 */
namespace VeyraStructureRules
{
	/**
	 * Whether Structure is invulnerable given All:
	 * - a lane's structures fall in order: while one nearer the enemy in its lane stands, it is;
	 * - base-defense towers are while all their team's inhibitors stand;
	 * - the Prime Well is unless every base tower of its team is destroyed and one of its inhibitors is down.
	 */
	VEYRAWORLD_API bool IsInvulnerable(const FVeyraStructureStatus& Structure, TConstArrayView<FVeyraStructureStatus> All);

	/** Whether Team's Prime Well regenerates: only while all its team's inhibitors stand (§18). */
	VEYRAWORLD_API bool PrimeWellRegenerates(EVeyraTeam Team, TConstArrayView<FVeyraStructureStatus> All);
}
