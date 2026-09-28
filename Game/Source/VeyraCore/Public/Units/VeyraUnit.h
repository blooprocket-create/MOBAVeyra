// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Misc/Optional.h"
#include "UObject/Interface.h"

#include "VeyraUnit.generated.h"

/**
 * What kind of unit something is. Rules that name a kind, such as Vanguard Combat State (Combat
 * Bible §28) or a skillshot that passes through minions (ADR-008 §9), ask this rather than a class.
 * Kinds arrive with their first unit: wildlife comes later.
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

	/** Whether Object is a structure. */
	VEYRACORE_API bool IsStructure(const UObject* Object);
}
