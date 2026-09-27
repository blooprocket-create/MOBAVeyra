// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Delivery/VeyraEffectDelivery.h"
#include "Shapes/VeyraShapes.h"
#include "Tuning/VeyraAbilitiesTuning.h"

class AActor;
class UAbilitySystemComponent;
class UWorld;

/** One zone of an area, with its effects prepared at Commit (Combat Bible §50). */
struct FVeyraPreparedZone
{
	FVeyraShape Shape;
	FVeyraPreparedEffects Effects;

	/** The shield the caster gains for each enemy Vanguard the zone catches, if it has one. */
	TOptional<FVeyraShieldGrant> CasterShieldPerVanguard;
};

/** How areas hit (ADR-008 §3, ADR-009 §4). Server only. */
namespace VeyraAreaDelivery
{
	/** Zones for Caster at Rank. */
	VEYRAABILITIES_API TArray<FVeyraPreparedZone> PrepareZones(UAbilitySystemComponent& Caster, TConstArrayView<FVeyraAreaZoneTuning> Zones, int32 Rank);

	/**
	 * Hits Caster's living enemies in the zones, placed at Frame's origin and facing, innermost first:
	 * each unit takes the first zone that touches it, and no other. A zone with a per-Vanguard caster
	 * shield grants it once for each enemy Vanguard it catches. Each hit is announced for Source's
	 * cast. Returns the units hit, nearest the origin first.
	 */
	VEYRAABILITIES_API TArray<AActor*> Resolve(UWorld& World, UAbilitySystemComponent& Caster, const FVeyraEffectFrame& Frame,
		TConstArrayView<FVeyraPreparedZone> Zones, const FVeyraAbilityHitSource& Source);
}
