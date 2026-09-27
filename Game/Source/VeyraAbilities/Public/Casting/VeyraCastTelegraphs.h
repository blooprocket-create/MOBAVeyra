// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Array.h"
#include "Shapes/VeyraShapes.h"

struct FVeyraAbilitiesTuning;
struct FVeyraCastState;

/**
 * Where a cast will land, for telegraphs on any machine (ADR-009 §4). The shapes come from the
 * ability's tuning and are placed by the rules its delivery uses, so a telegraph matches the hit.
 */
namespace VeyraCastTelegraphs
{
	/**
	 * The shapes the cast in State lands in, for a caster whose body is now at CasterLocation with
	 * radius CasterRadius:
	 * - an area's zones, innermost first;
	 * - a skillshot's path, as long as its range and as wide as its projectile;
	 * - a dash's start zones, then its path, as wide as the caster;
	 * - a buff's aura.
	 * Empty when the ability shows nothing (a targeted spell, an empowered attack) or is unknown.
	 */
	VEYRAABILITIES_API TArray<FVeyraPlacedShape> ForCast(const FVeyraAbilitiesTuning& Tuning, const FVeyraCastState& State, const FVector& CasterLocation,
		double CasterRadius);
}
