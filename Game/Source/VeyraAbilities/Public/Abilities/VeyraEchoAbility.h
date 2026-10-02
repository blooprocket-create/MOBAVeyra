// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Abilities/VeyraGameplayAbility.h"

#include "VeyraEchoAbility.generated.h"

/**
 * The Echo archetype (ADR-050 §4), as Project Echo: it forms its caster's Echo at the cast's point, through
 * UVeyraEchoSubsystem. A manifest Echo waits to repeat its caster's next eligible ability. Each ability of this kind is
 * an entry in Abilities.json's echo map; items carry them as their Actives.
 */
UCLASS()
class VEYRAABILITIES_API UVeyraEchoAbility : public UVeyraGameplayAbility
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
