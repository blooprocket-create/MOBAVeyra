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
}
