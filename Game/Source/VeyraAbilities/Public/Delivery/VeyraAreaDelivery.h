// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Delivery/VeyraEffectDelivery.h"
#include "Shapes/VeyraShapes.h"
#include "Tuning/VeyraAbilitiesTuning.h"

class AActor;
class AVeyraLingeringArea;
class UAbilitySystemComponent;
class UWorld;

/** What a zone does for its caster's allied Vanguards, prepared at Commit (ADR-035 §4). */
struct FVeyraPreparedAllyEffects
{
	/** The Health each regains from its caster; 0 for none. */
	double Heal = 0.0;
	TArray<FVeyraStatusSpec> Statuses;
	EVeyraAllyReach Reach = EVeyraAllyReach::OthersOnly;
};

/** One zone of an area, with its effects prepared at Commit (Combat Bible §50). */
struct FVeyraPreparedZone
{
	FVeyraShape Shape;
	FVeyraPreparedEffects Effects;

	/** The shield the caster gains for each enemy Vanguard the zone catches, if it has one. */
	TOptional<FVeyraShieldGrant> CasterShieldPerVanguard;

	/** The statuses the caster gains for each enemy Vanguard the zone catches. */
	TArray<FVeyraStatusSpec> CasterStatusesPerVanguard;

	/** What it does for its caster's allies in its shape, prepared at Commit, if anything (ADR-035 §4). */
	TOptional<FVeyraPreparedAllyEffects> AllyEffects;
};

/** The statuses a lingering area gives each side inside it, from its caster's Level at Commit (ADR-018 §5). */
struct FVeyraLingerStatuses
{
	TArray<FVeyraStatusSpec> Caster;
	TArray<FVeyraStatusSpec> Allies;
	TArray<FVeyraStatusSpec> Enemies;
};

/** What a lingering area does to the enemy units inside it beside its statuses, prepared at Commit (ADR-026 §4). */
struct FVeyraLingerEffects
{
	/** Dealt at each pulse after it lands; empty when its pulses give statuses only. */
	TArray<FVeyraPreparedZone> Pulse;

	/** Dealt as it ends, measured from its centre; empty when its end does nothing. */
	TArray<FVeyraPreparedZone> End;

	/** How long before its end the presentation marks it. */
	double EndWarningSeconds = 0.0;

	/** The cast they belong to, for the hits' announcements. */
	int32 CastId = 0;
};

/** An area's lingering area, prepared at its cast's Commit to be armed where the area lands (ADR-027 §6). */
struct FVeyraPreparedLinger
{
	/** Its area's outermost zone's shape. */
	FVeyraShape Shape;
	FVeyraLingerStatuses Statuses;
	FVeyraLingerEffects Effects;
	double DurationSeconds = 0.0;
	double PulseSeconds = 0.0;
	EVeyraLingerSight Sight = EVeyraLingerSight::None;
	FVeyraContentId Ability;

	/** The pull of the movement field it holds; 0 for none (ADR-033 §5). */
	double FieldPull = 0.0;
};

/** How areas hit (ADR-008 §3, ADR-009 §4). Server only, except Place. */
namespace VeyraAreaDelivery
{
	/**
	 * Where Area lands for a caster at CasterLocation casting at Point in Direction: on the caster,
	 * facing the direction, or on the point, facing away from the caster. Any machine: telegraphs
	 * place an area as the server places its hit.
	 */
	VEYRAABILITIES_API FVeyraEffectFrame Place(const FVeyraAreaAbilityTuning& Area, const FVector& CasterLocation, const FVector& Point, const FVector& Direction);

	/**
	 * Server only: the delay of Area landing at Point for Caster: the first delayWithin entry whose
	 * lingering area, Caster's own of the named ability, holds the point, else Area's own (ADR-026 §4).
	 */
	VEYRAABILITIES_API double DelayAt(const UWorld& World, const UAbilitySystemComponent& Caster, const FVeyraAreaAbilityTuning& Area, const FVector& Point);

	/** Server only: Caster's own lingering area of Ability, if one stands (ADR-028 §5). */
	VEYRAABILITIES_API AVeyraLingeringArea* FindCastersLingeringArea(const UWorld& World, const UAbilitySystemComponent& Caster, const FVeyraContentId& Ability);

	/** Zones for Caster at Rank. */
	VEYRAABILITIES_API TArray<FVeyraPreparedZone> PrepareZones(UAbilitySystemComponent& Caster, TConstArrayView<FVeyraAreaZoneTuning> Zones, int32 Rank);

	/** What Caster does for its allies at Rank, from its power now (ADR-035 §4). */
	VEYRAABILITIES_API FVeyraPreparedAllyEffects PrepareAllyEffects(const UAbilitySystemComponent& Caster, const FVeyraZoneAllyEffectsTuning& Allies, int32 Rank);

	/** Whether Unit is an allied Vanguard of Caster's that Help reaches: living, on its side, and Caster's own body only for CasterToo. */
	VEYRAABILITIES_API bool Reaches(const UAbilitySystemComponent& Caster, const AActor& Unit, const FVeyraPreparedAllyEffects& Help);

	/** Server: Caster's heal and statuses for Ally, the heal through every rule of restoration (Combat Bible §6). */
	VEYRAABILITIES_API void HelpAlly(UAbilitySystemComponent& Caster, AActor& Ally, const FVeyraPreparedAllyEffects& Help);

	/**
	 * Hits Caster's living enemies in the zones, placed at Frame's origin and facing, innermost first:
	 * each unit takes the first zone that touches it, and no other. A zone with a per-Vanguard caster
	 * shield grants it once for each enemy Vanguard it catches. Each hit is announced for Source's
	 * cast. Returns the units hit, nearest the origin first.
	 */
	VEYRAABILITIES_API TArray<AActor*> Resolve(UWorld& World, UAbilitySystemComponent& Caster, const FVeyraEffectFrame& Frame,
		TConstArrayView<FVeyraPreparedZone> Zones, const FVeyraAbilityHitSource& Source);

	/**
	 * Area's lingering area for Caster's cast of Ability, CastId, at Rank: its statuses from the caster's
	 * Level, its pulses and end from the caster's power, both now (Combat Bible §50). None when it has none.
	 */
	VEYRAABILITIES_API TOptional<FVeyraPreparedLinger> PrepareLinger(UAbilitySystemComponent& Caster, const FVeyraAreaAbilityTuning& Area, int32 Rank,
		int32 Level, const FVeyraContentId& Ability, int32 CastId);

	/** Server only: arms Linger at Placement, and lights it for Caster's side when its sight is ordinary (ADR-016 §5). */
	VEYRAABILITIES_API void ArmLinger(UWorld& World, UAbilitySystemComponent& Caster, const FVeyraEffectFrame& Placement, const FVeyraPreparedLinger& Linger);
}
