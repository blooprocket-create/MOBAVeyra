// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Abilities/VeyraGameplayAbility.h"

#include "VeyraPlacementAbility.generated.h"


/**
 * The placement archetype (ADR-031 §4), as Angeru's False Body: it places one of its caster's markers
 * at a point within its cast range, on the nearest navigable ground, in place of any it placed before.
 * A follow-up its cast opens ends with the marker (ADR-032 §5). Each ability of this kind is an entry in
 * Abilities.json's placement map.
 */
UCLASS()
class VEYRAABILITIES_API UVeyraPlacementAbility : public UVeyraGameplayAbility
{
	GENERATED_BODY()

protected:
	virtual bool Defines(const FVeyraContentId& Ability) const override;
	virtual double GetResourceCost(const FVeyraContentId& Ability, int32 Rank) const override;
	virtual double GetCooldownSeconds(const FVeyraContentId& Ability, int32 Rank) const override;
	virtual EVeyraCastRejection CheckTarget(const AActor& Caster, const FVeyraContentId& Ability, const FVeyraCastTarget& Target) const override;
	virtual const FVeyraCastTuning* GetCastTuning(const FVeyraContentId& Ability) const override;
	virtual FVeyraChannelPlan Deliver(const FVeyraCast& Cast) override;
	virtual bool IsOffensive(const FVeyraContentId& Ability) const override;
};
