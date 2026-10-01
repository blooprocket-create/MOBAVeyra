// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Abilities/VeyraGameplayAbility.h"

#include "VeyraSkillshotAbility.generated.h"

/**
 * The archetype for an ability that fires a line projectile toward a ground point (ADR-008 §3). Its
 * effects are prepared at Commit and land on what the projectile hits; terrain stops it (§9). Each
 * ability of this kind is an entry in Abilities.json's skillshot map.
 */
UCLASS()
class VEYRAABILITIES_API UVeyraSkillshotAbility : public UVeyraGameplayAbility
{
	GENERATED_BODY()

protected:
	virtual bool Defines(const FVeyraContentId& Ability) const override;
	virtual double GetResourceCost(const FVeyraContentId& Ability, int32 Rank) const override;
	virtual double GetCooldownSeconds(const FVeyraContentId& Ability, int32 Rank) const override;
	virtual EVeyraCastRejection CheckTarget(const AActor& Caster, const FVeyraContentId& Ability, const FVeyraCastTarget& Target) const override;
	virtual const FVeyraCastTuning* GetCastTuning(const FVeyraContentId& Ability) const override;
	/** One that recoils its caster moves it, as Kade's Reposition does. */
	virtual bool MovesCaster(const FVeyraContentId& Ability) const override;
	virtual FVeyraChannelPlan Deliver(const FVeyraCast& Cast) override;
};
