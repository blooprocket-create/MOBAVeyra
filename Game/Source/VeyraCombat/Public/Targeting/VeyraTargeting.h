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
	/** On the caster's side, or on no side, for an ability that targets enemies. */
	NotHostile,
	/** Further than the cast range, plus the server's tolerance, edge to edge. */
	OutOfRange,
	/** A structure, for an ability that cannot damage structures (Combat Bible §33). */
	Structure,
	/**
	 * Hidden from the caster: in fog of war, stealthed, or an enemy Vanguard inside Dense Fog the
	 * caster is not inside (Vision Bible §1, §2; Combat Bible §11; ADR-016 §2).
	 */
	NotVisible,
	/** A ward, for an ability: only basic attacks strike wards (ADR-016 §6). */
	Ward,
	/** Not a Vanguard on the caster's side, for an ability that targets allies (ADR-027 §4). */
	NotAllied,
};

/** Whether a targeted action may pick a structure (Combat Bible §33). */
enum class EVeyraStructureTargeting : uint8
{
	/** An ordinary ability: structures are not valid targets. */
	Refuse,
	/** A basic attack or a structure-enabled ability. */
	Allow,
};

/**
 * Combat's rules for who may target whom (Combat Bible §29, §30, §40). Abilities ask them; they
 * never re-derive range or hostility. Whether the caster can see the target is Vision's, read
 * through Core's visibility contract (ADR-016 §2).
 */
namespace VeyraTargeting
{
	/** Whether Unit is a combatant that is alive. A pawn counts through its PlayerState. */
	VEYRACOMBAT_API bool IsAlive(const AActor* Unit);

	/**
	 * Whether A and B are hostile: on opposing sides, or one a neutral unit (wildlife or an objective)
	 * and the other on a side but neither a Fluxborn nor a structure (ADR-014 §1). Anything else on no
	 * side is hostile to nothing: neutral units are an explicit targeting category, not implicit
	 * enemies (Combat Bible §29).
	 */
	VEYRACOMBAT_API bool AreHostile(const UObject* A, const UObject* B);

	/**
	 * Whether Acquirer, a unit or anything with a side, may acquire Target as a target now: whether
	 * the world's vision lets it see Target (ADR-016 §2), and, for an enemy, whether Target is not
	 * Untargetable (Combat Bible §10; ADR-030 §2). A world without vision allows every visible target.
	 */
	VEYRACOMBAT_API bool CanAcquire(const UObject* Acquirer, const AActor& Target);

	/** Whether Unit holds an Untargetable status now (Combat Bible §10). */
	VEYRACOMBAT_API bool IsUntargetable(const AActor& Unit);

	/**
	 * Whether Unit is Source's enemy and Source's new hits may land on it: an Untargetable unit takes no
	 * skillshot, area, cleave or contact of its enemies' (Combat Bible §10; ADR-030 §2).
	 */
	VEYRACOMBAT_API bool CanHitEnemy(const UObject* Source, const AActor& Unit);

	/** Distance between two units' collision edges on the ground plane, never below 0 (Combat Bible §40). */
	VEYRACOMBAT_API double EdgeToEdgeDistance(const AActor& A, const AActor& B);

	/** Whether Target is within CastRange of Caster, with the server's tuned latency tolerance (§30). */
	VEYRACOMBAT_API bool IsWithinCastRange(const AActor& Caster, const AActor& Target, double CastRange);

	/**
	 * Whether Caster may target Target with a targeted action against enemies, at CastRange. A
	 * structure is valid only when Structures allows it, and so is a ward, which of those actions
	 * only a Vanguard's basic attack harms.
	 */
	VEYRACOMBAT_API EVeyraTargetValidity CheckEnemyTarget(const AActor& Caster, const AActor* Target, double CastRange,
		EVeyraStructureTargeting Structures = EVeyraStructureTargeting::Refuse);

	/**
	 * Whether Caster may target Target with a targeted action for allies, at CastRange: a living Vanguard
	 * on its side other than itself (ADR-027 §4). Allies are always seen, so vision does not enter it.
	 */
	VEYRACOMBAT_API EVeyraTargetValidity CheckAllyTarget(const AActor& Caster, const AActor* Target, double CastRange);
}
