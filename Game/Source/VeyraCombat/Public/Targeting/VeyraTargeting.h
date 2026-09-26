// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;
class UObject;

/** Why a unit may not be the target of a targeted ability, or Valid (Combat Bible §29, §30). */
enum class EVeyraTargetValidity : uint8
{
	Valid,
	/** Nothing, or something that is not a combatant. */
	NotACombatant,
	/** The caster itself. */
	Caster,
	/** A combatant whose death is final. */
	Dead,
	/** On the caster's side, for an ability that targets enemies. */
	NotHostile,
	/** Further than the cast range, plus the server's tolerance, edge to edge. */
	OutOfRange,
};

/**
 * Combat's rules for who may target whom (Combat Bible §29, §30, §40). Abilities ask them; they
 * never re-derive range or hostility. Whether the caster can see the target is Vision's to add.
 */
namespace VeyraTargeting
{
	/** Whether Unit is a combatant that is alive. A pawn counts through its PlayerState. */
	VEYRACOMBAT_API bool IsAlive(const AActor* Unit);

	/** Whether A and B are on different sides. */
	VEYRACOMBAT_API bool AreHostile(const UObject* A, const UObject* B);

	/** Distance between two units' collision edges on the ground plane, never below 0 (Combat Bible §40). */
	VEYRACOMBAT_API double EdgeToEdgeDistance(const AActor& A, const AActor& B);

	/** Whether Target is within CastRange of Caster, with the server's tuned latency tolerance (§30). */
	VEYRACOMBAT_API bool IsWithinCastRange(const AActor& Caster, const AActor& Target, double CastRange);

	/** Whether Caster may target Target with a targeted ability against enemies, at CastRange. */
	VEYRACOMBAT_API EVeyraTargetValidity CheckEnemyTarget(const AActor& Caster, const AActor* Target, double CastRange);
}
