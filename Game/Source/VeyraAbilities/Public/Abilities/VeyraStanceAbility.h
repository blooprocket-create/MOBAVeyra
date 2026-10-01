// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Abilities/VeyraGameplayAbility.h"

#include "VeyraStanceAbility.generated.h"

/**
 * The stance archetype (ADR-031 §3), as Angeru's Forsake the Schools: casting it puts its abilities in
 * the slots it names, each slot's ability stowed with its cooldown; casting it again puts the stowed
 * ones back. Each slot keeps its rank across both, and each ability its own cooldown. Each ability of
 * this kind is an entry in Abilities.json's stance map.
 */
UCLASS()
class VEYRAABILITIES_API UVeyraStanceAbility : public UVeyraGameplayAbility
{
	GENERATED_BODY()

protected:
	virtual bool Defines(const FVeyraContentId& Ability) const override;
	virtual double GetResourceCost(const FVeyraContentId& Ability, int32 Rank) const override;
	virtual double GetCooldownSeconds(const FVeyraContentId& Ability, int32 Rank) const override;
	virtual const FVeyraCastTuning* GetCastTuning(const FVeyraContentId& Ability) const override;
	virtual FVeyraChannelPlan Deliver(const FVeyraCast& Cast) override;
	virtual bool IsOffensive(const FVeyraContentId& Ability) const override;
};
