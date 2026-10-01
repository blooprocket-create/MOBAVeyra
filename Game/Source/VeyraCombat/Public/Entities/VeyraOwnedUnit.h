// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "UObject/Interface.h"

#include "VeyraOwnedUnit.generated.h"

class UAbilitySystemComponent;

UINTERFACE(MinimalAPI, NotBlueprintable)
class UVeyraOwnedUnit : public UInterface
{
	GENERATED_BODY()
};

/**
 * A unit that belongs to a Vanguard (ADR-003; ADR-034 §1): a placed marker or a companion. What it causes
 * is its owner's (Combat Bible §32): VeyraCombat::ResponsibleFor follows it to its owner wherever Combat
 * attributes damage, contributions and kills, while what it inherits stays what it declares.
 */
class IVeyraOwnedUnit
{
	GENERATED_BODY()

public:
	/** Its owner's Ability System Component: a participant's; null once its owner is gone. */
	virtual UAbilitySystemComponent* GetOwnerAbilities() const = 0;
};
