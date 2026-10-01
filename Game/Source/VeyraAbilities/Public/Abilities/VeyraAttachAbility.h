// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Abilities/VeyraGameplayAbility.h"
#include "Delivery/VeyraEffectDelivery.h"

#include "VeyraAttachAbility.generated.h"

class UVeyraMovementComponent;
struct FVeyraAttachEnd;
struct FVeyraDashEnd;

/**
 * The archetype for an ability that leaps its caster at an enemy and holds on to it (ADR-018 §2), as
 * Patch's Bear Hug. The leap is a dash toward the target; ending within reach of it, the caster
 * attaches, the host takes the host effects and holds the host statuses while the caster holds on.
 * A leap that ends out of reach does nothing more, and its cooldown stands (ADR-018 §8). Its recast,
 * if its cast declares one, lasts only while the caster holds on. Each ability of this kind is an
 * entry in Abilities.json's attach map.
 */
UCLASS()
class VEYRAABILITIES_API UVeyraAttachAbility : public UVeyraGameplayAbility
{
	GENERATED_BODY()

protected:
	virtual bool Defines(const FVeyraContentId& Ability) const override;
	virtual bool MovesCaster(const FVeyraContentId& Ability) const override { return Defines(Ability); }
	virtual double GetResourceCost(const FVeyraContentId& Ability, int32 Rank) const override;
	virtual double GetCooldownSeconds(const FVeyraContentId& Ability, int32 Rank) const override;
	virtual EVeyraCastRejection CheckTarget(const AActor& Caster, const FVeyraContentId& Ability, const FVeyraCastTarget& Target) const override;
	virtual const FVeyraCastTuning* GetCastTuning(const FVeyraContentId& Ability) const override;
	virtual FVeyraChannelPlan Deliver(const FVeyraCast& Cast) override;

private:
	/** A leap under way, or a hold, and what taking hold does. */
	struct FGrab
	{
		TWeakObjectPtr<UAbilitySystemComponent> Caster;
		TWeakObjectPtr<AActor> Target;
		FVeyraContentId Ability;
		FVeyraAbilityHitSource Source;
		FVeyraPreparedEffects Effects;
		TArray<FVeyraStatusSpec> HostStatuses;
		bool bHolding = false;
	};

	void OnLeapEnded(const FVeyraDashEnd& End);

	/** The leap ended: it takes hold if the target is within reach, and misses otherwise. */
	void TakeHold();

	void OnAttachEnded(const FVeyraAttachEnd& End);

	/** It missed, or let go other than by its recast: its recast has nothing to act on. */
	void Miss();

	void StopWatching();

	TOptional<FGrab> Grab;
	TWeakObjectPtr<UVeyraMovementComponent> Watched;
	FDelegateHandle LeapEndedHandle;
	FDelegateHandle AttachEndedHandle;
};
