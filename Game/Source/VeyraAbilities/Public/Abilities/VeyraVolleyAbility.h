// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Abilities/VeyraGameplayAbility.h"

#include "VeyraVolleyAbility.generated.h"

/**
 * The archetype for a volley (ADR-018 §6): as it commits it opens a lane in its direction, plants its
 * caster, and its slot holds its shot until the shots or the time run out. Each ability of this kind
 * is an entry in Abilities.json's volley map; UVeyraVolleySubsystem keeps each lane.
 */
UCLASS()
class VEYRAABILITIES_API UVeyraVolleyAbility : public UVeyraGameplayAbility
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
