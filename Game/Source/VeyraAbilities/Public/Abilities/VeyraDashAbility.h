// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Abilities/VeyraGameplayAbility.h"
#include "Delivery/VeyraEffectDelivery.h"

#include "VeyraDashAbility.generated.h"

class UVeyraMovementComponent;
struct FVeyraDashEnd;

/**
 * The archetype for an ability that moves its caster toward or away from a ground point (ADR-008 §3;
 * Combat Bible §9): areas as it sets off, then a Dash that terrain stops and displacement interrupts,
 * and effects on the enemy it stops at. Each ability of this kind is an entry in Abilities.json's
 * dash map.
 */
UCLASS()
class VEYRAABILITIES_API UVeyraDashAbility : public UVeyraGameplayAbility
{
	GENERATED_BODY()

protected:
	virtual bool Defines(const FVeyraContentId& Ability) const override;
	virtual double GetResourceCost(const FVeyraContentId& Ability, int32 Rank) const override;
	virtual double GetCooldownSeconds(const FVeyraContentId& Ability, int32 Rank) const override;
	virtual EVeyraCastRejection CheckTarget(const AActor& Caster, const FVeyraContentId& Ability, const FVeyraCastTarget& Target) const override;
	virtual const FVeyraCastTuning* GetCastTuning(const FVeyraContentId& Ability) const override;
	virtual FVeyraChannelPlan Deliver(const FVeyraCast& Cast) override;

private:
	/** What a dash that stops at an enemy does there, prepared at Commit (Combat Bible §50). */
	struct FPendingContact
	{
		TWeakObjectPtr<UAbilitySystemComponent> Caster;
		FVeyraAbilityHitSource Source;
		FVector Direction = FVector::ForwardVector;
		FVeyraPreparedEffects Effects;
		TArray<FVeyraStatusSpec> SelfStatuses;
	};

	void OnDashEnded(const FVeyraDashEnd& End);
	void StopWatching();

	/** The dash in progress that may stop at an enemy, and the movement running it. */
	TOptional<FPendingContact> Contact;
	TWeakObjectPtr<UVeyraMovementComponent> Watched;
	FDelegateHandle DashEndedHandle;
};
