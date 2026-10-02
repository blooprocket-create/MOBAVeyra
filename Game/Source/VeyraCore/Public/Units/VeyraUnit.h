// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Misc/Optional.h"
#include "UObject/Interface.h"

#include "VeyraUnit.generated.h"

/**
 * What kind of unit something is. Rules that name a kind, such as Vanguard Combat State (Combat
 * Bible §28) or a skillshot that passes through minions (ADR-008 §9), ask this rather than a class.
 */
UENUM()
enum class EVeyraUnitKind : uint8
{
	/** A player's or bot's champion. */
	Vanguard,
	/** A lane minion (Battleground Bible). */
	Fluxborn,
	/**
	 * A lane Spire, base-defense tower, inhibitor or Prime Well (Battleground Bible §5, §18). Combat
	 * Bible §33's structure rules apply to it: basic attacks and tower attacks damage it, abilities
	 * do not unless they say so (ADR-011 §5).
	 */
	Structure,
	/** A creature of a jungle camp (Battleground Bible §8). Neutral: on no side (ADR-014 §1). */
	Wildlife,
	/** A neutral objective, such as a Flux Well (Battleground Bible §6). On no side, and not a structure (ADR-014 §4). */
	Objective,
	/**
	 * A placed ward (Vision Bible §4). It counts hits, not damage: only a Vanguard's basic attack
	 * reaches it, one point of Health each; abilities, areas and skillshots pass it by (ADR-016 §6).
	 */
	Ward,
	/**
	 * A placed marker a Vanguard's ability leaves (ADR-003; ADR-030 §5), such as an illusion. It counts
	 * hits as a ward does, but anything of its enemies' may hit it; no status affects it.
	 */
	Marker,
	/**
	 * A Vanguard's companion (ADR-003's combat entity; ADR-034 §1), such as Nix: an owned unit with its
	 * own Health and behaviour, on its owner's side. What it causes is its owner's (Combat Bible §32).
	 */
	Companion,
	/**
	 * A Vanguard's Echo (Item Bible §9, §11; ADR-050 §2): a projection of its holder, on its holder's side, whose
	 * sealed Health is its Integrity. What it causes is its holder's (Combat Bible §32).
	 */
	Echo,
};

UINTERFACE(MinimalAPI, NotBlueprintable)
class UVeyraUnit : public UInterface
{
	GENERATED_BODY()
};

/** Anything that is a unit in combat: a participant's PlayerState and its Vanguard, later minions. */
class IVeyraUnit
{
	GENERATED_BODY()

public:
	virtual EVeyraUnitKind GetVeyraUnitKind() const = 0;
};

namespace VeyraUnits
{
	/** The kind of any object that declares one, and nothing for everything else. */
	VEYRACORE_API TOptional<EVeyraUnitKind> KindOf(const UObject* Object);

	/** Whether Object is a Vanguard. */
	VEYRACORE_API bool IsVanguard(const UObject* Object);

	/**
	 * Whether Object is a neutral unit, wildlife or an objective: on no side, and hostile to what is on
	 * a side except Fluxborn and structures (ADR-014 §1).
	 */
	VEYRACORE_API bool IsNeutral(const UObject* Object);

	/** Whether Object is a structure. */
	VEYRACORE_API bool IsStructure(const UObject* Object);

	/** Whether Object is a ward. */
	VEYRACORE_API bool IsWard(const UObject* Object);

	/** Whether Object is a placed marker. */
	VEYRACORE_API bool IsMarker(const UObject* Object);
}
