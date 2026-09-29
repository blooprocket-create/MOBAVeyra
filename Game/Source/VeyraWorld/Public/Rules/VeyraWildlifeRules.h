// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Array.h"
#include "Math/Vector2D.h"

/**
 * The jungle's rules as pure functions (Battleground Bible §8, §17; ADR-014 §2), so they are tested
 * without a world.
 */
namespace VeyraWildlifeRules
{
	/** Where each of a camp's Count creatures stands: at its centre alone, or evenly round it at Spacing. */
	VEYRAWORLD_API TArray<FVector2D> Positions(const FVector2D& Center, int32 Count, double Spacing);

	/**
	 * Whether Point lies within a camp's leash: within LeashRadius of the camp's centre, which is what
	 * World.json's validation keeps clear of the lanes and the river.
	 */
	VEYRAWORLD_API bool IsWithinLeash(const FVector2D& Center, double LeashRadius, const FVector2D& Point);

	/**
	 * Whether a creature keeps fighting its target (§17): the target is still one it may fight, and
	 * both it and its target are within its leash. Otherwise it walks home and heals.
	 */
	VEYRAWORLD_API bool KeepsFighting(const FVector2D& Center, double LeashRadius, const FVector2D& Self, const FVector2D& Target, bool bTargetValid);
}
