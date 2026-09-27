// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Misc/Optional.h"
#include "Shapes/VeyraShapes.h"
#include "Statuses/VeyraStatusTypes.h"
#include "Tuning/VeyraAbilitiesTuning.h"
#include "VeyraCombatVerbs.h"

class AActor;
class UAbilitySystemComponent;
class UWorld;

/** Where an area lands: its origin, and the direction its shapes face. */
struct FVeyraAreaPlacement
{
	FVector Origin = FVector::ZeroVector;
	FVector Direction = FVector::ForwardVector;

	/** Whether the origin is the caster, so a Pull toward it stops at the caster's edge. */
	bool bOriginIsCaster = false;
};

/** One zone of an area, with its effects prepared at Commit (Combat Bible §50). */
struct FVeyraPreparedZone
{
	FVeyraShape Shape;

	/** Invalid when the zone deals no damage. */
	FVeyraPreparedDamage Damage;

	TArray<FVeyraStatusSpec> Statuses;
	TOptional<FVeyraDisplacementTuning> Displacement;
};

/** How areas hit (ADR-008 §3, ADR-009 §4). Server only. */
namespace VeyraAreaDelivery
{
	/** Area's zones for Caster at Rank: damage from the caster's power now, statuses and displacement from data. */
	VEYRAABILITIES_API TArray<FVeyraPreparedZone> PrepareZones(UAbilitySystemComponent& Caster, const FVeyraAreaAbilityTuning& Area, int32 Rank);

	/**
	 * Hits Caster's living enemies in the zones, innermost first: each unit takes the first zone that
	 * touches it, and no other. Returns the units hit, nearest the origin first.
	 */
	VEYRAABILITIES_API TArray<AActor*> Resolve(UWorld& World, UAbilitySystemComponent& Caster, const FVeyraAreaPlacement& Placement,
		TConstArrayView<FVeyraPreparedZone> Zones);
}
