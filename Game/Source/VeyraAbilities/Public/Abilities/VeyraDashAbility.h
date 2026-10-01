// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Abilities/VeyraGameplayAbility.h"
#include "Delivery/VeyraAreaDelivery.h"
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
	virtual bool MovesCaster(const FVeyraContentId& Ability) const override { return Defines(Ability); }
	virtual bool TakesOverDash(const FVeyraContentId& Ability) const override;
	virtual double GetResourceCost(const FVeyraContentId& Ability, int32 Rank) const override;
	virtual double GetCooldownSeconds(const FVeyraContentId& Ability, int32 Rank) const override;
	virtual EVeyraCastRejection CheckTarget(const AActor& Caster, const FVeyraContentId& Ability, const FVeyraCastTarget& Target) const override;
	virtual const FVeyraCastTuning* GetCastTuning(const FVeyraContentId& Ability) const override;
	virtual FVeyraChannelPlan Deliver(const FVeyraCast& Cast) override;
	virtual bool IsOffensive(const FVeyraContentId& Ability) const override;

private:
	/**
	 * What a dash does at the enemy it stops at and where it lands, prepared at Commit (Combat Bible
	 * §50). It outlives the ability that set off, which a used-once follow-up's removal may end mid-dash,
	 * so a lambda bound to the caster holds it (ADR-018 §1).
	 */
	struct FPendingContact
	{
		TWeakObjectPtr<UAbilitySystemComponent> Caster;
		FVeyraAbilityHitSource Source;
		FVector Direction = FVector::ForwardVector;
		FVeyraPreparedEffects Effects;
		TArray<FVeyraStatusSpec> SelfStatuses;
		TArray<FVeyraPreparedZone> EndZones;
		/** Whether it leaves its caster's ride as it lands. */
		bool bLeavesRide = false;
		TWeakObjectPtr<UVeyraMovementComponent> Movement;
		FDelegateHandle Handle;
	};

	/** The dash ended: its landing, its ride's end, and its effects on the enemy it stopped at. */
	static void Land(const FPendingContact& Pending, const FVeyraDashEnd& End);
};
