// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Abilities/VeyraGameplayAbility.h"

#include "VeyraEmpoweredAttackAbility.generated.h"

/**
 * The archetype for an ability that empowers its caster's next basic attack (ADR-008 §3; Combat
 * Bible §17). The attack stays a basic attack: it triggers On Attack and On Hit, and its extra damage
 * joins its own damage event. Each ability of this kind is an entry in Abilities.json's
 * empoweredAttack map.
 */
UCLASS()
class VEYRAABILITIES_API UVeyraEmpoweredAttackAbility : public UVeyraGameplayAbility
{
	GENERATED_BODY()

protected:
	virtual bool Defines(const FVeyraContentId& Ability) const override;
	virtual double GetResourceCost(const FVeyraContentId& Ability, int32 Rank) const override;
	virtual double GetCooldownSeconds(const FVeyraContentId& Ability, int32 Rank) const override;
	virtual const FVeyraCastTuning* GetCastTuning(const FVeyraContentId& Ability) const override;
	virtual FVeyraChannelPlan Deliver(const FVeyraCast& Cast) override;
};
