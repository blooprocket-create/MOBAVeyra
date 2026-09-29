// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Abilities/VeyraGameplayAbility.h"

#include "VeyraTetherAbility.generated.h"

/**
 * The archetype for an ability that tethers an enemy to its caster (Combat Bible §43; ADR-018), as
 * Patch's Don't Leave Me. Combat's tether ledger keeps the link: the caster's side sees the target
 * while it holds, and stretched beyond its range it may snap the target back once, and ends. Each
 * ability of this kind is an entry in Abilities.json's tether map.
 */
UCLASS()
class VEYRAABILITIES_API UVeyraTetherAbility : public UVeyraGameplayAbility
{
	GENERATED_BODY()

protected:
	virtual bool Defines(const FVeyraContentId& Ability) const override;
	virtual double GetResourceCost(const FVeyraContentId& Ability, int32 Rank) const override;
	virtual double GetCooldownSeconds(const FVeyraContentId& Ability, int32 Rank) const override;
	virtual EVeyraCastRejection CheckTarget(const AActor& Caster, const FVeyraContentId& Ability, const FVeyraCastTarget& Target) const override;
	virtual const FVeyraCastTuning* GetCastTuning(const FVeyraContentId& Ability) const override;
	virtual FVeyraChannelPlan Deliver(const FVeyraCast& Cast) override;
};
