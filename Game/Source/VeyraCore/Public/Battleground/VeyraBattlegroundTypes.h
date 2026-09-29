// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "UObject/ObjectMacros.h"

#include "VeyraBattlegroundTypes.generated.h"

/**
 * The battleground's structures (Battleground Bible §3, §5, §10, §18). World spawns and runs them;
 * Economy, Flux, Match and presentation name them, so the vocabulary lives here (ADR-011 §2).
 */
UENUM()
enum class EVeyraStructureKind : uint8
{
	/** A Corrupted Spire, three per lane (§5). */
	LaneSpire,
	/** One of the two base-defense towers before the Prime Well (§18). */
	BaseTower,
	/** A lane's reconstructing inhibitor, beyond its Spires (§10). */
	Inhibitor,
	/** The passive Prime Well; destroying the enemy's wins the match (§3). */
	PrimeWell,
};

/** The three Fluxways (Battleground Bible §2, §4). */
UENUM()
enum class EVeyraLane : uint8
{
	Top,
	Mid,
	Bottom,
};
