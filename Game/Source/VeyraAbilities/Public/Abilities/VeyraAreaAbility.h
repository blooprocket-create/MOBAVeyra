// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Abilities/VeyraGameplayAbility.h"
#include "Delivery/VeyraAreaDelivery.h"

#include "VeyraAreaAbility.generated.h"

/**
 * The archetype for an ability that hits the enemies in shapes at the caster or a ground point
 * (ADR-008 §3): at once, after a telegraphed delay, or in channel ticks, in zones ordered innermost
 * first. Each ability of this kind is an entry in Abilities.json's area map.
 */
UCLASS()
class VEYRAABILITIES_API UVeyraAreaAbility : public UVeyraGameplayAbility
{
	GENERATED_BODY()

protected:
	virtual bool Defines(const FVeyraContentId& Ability) const override;
	virtual double GetResourceCost(const FVeyraContentId& Ability, int32 Rank) const override;
	virtual double GetCooldownSeconds(const FVeyraContentId& Ability, int32 Rank) const override;
	virtual EVeyraCastRejection CheckTarget(const AActor& Caster, const FVeyraContentId& Ability, const FVeyraCastTarget& Target) const override;
	virtual const FVeyraCastTuning* GetCastTuning(const FVeyraContentId& Ability) const override;
	virtual FVeyraChannelPlan Deliver(const FVeyraCast& Cast) override;
	virtual void DeliverChannelTick(const FVeyraCast& Cast, int32 Tick) override;

private:
	/** A channelled area's placement and zones, from Commit to its last tick. */
	FVeyraAreaPlacement ChannelPlacement;
	TArray<FVeyraPreparedZone> ChannelZones;
};
