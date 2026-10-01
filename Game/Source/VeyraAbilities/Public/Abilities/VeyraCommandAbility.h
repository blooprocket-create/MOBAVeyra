// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Abilities/VeyraGameplayAbility.h"
#include "Engine/TimerHandle.h"

#include "VeyraCommandAbility.generated.h"

/**
 * The command archetype (ADR-034 §5), as Marek's Hunt: it sends its caster's companion to a ground point,
 * where the companion leaps, lands its zones as its own hit and holds; its follow-up recalls the companion
 * to its owner's side. Each ability of this kind is an entry in Abilities.json's command map.
 */
UCLASS()
class VEYRAABILITIES_API UVeyraCommandAbility : public UVeyraGameplayAbility
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

private:
	/** A leap under way: where its companion lands its zones once it arrives. */
	FTimerHandle LandingTimer;
};
