// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Abilities/VeyraGameplayAbility.h"
#include "Engine/TimerHandle.h"

#include "VeyraSelfBuffAbility.generated.h"

/**
 * The archetype for an ability that buffs its caster (ADR-008 §3): statuses and a shield on the
 * caster, and optionally an aura of statuses for nearby allied Vanguards, refreshed on an interval
 * (ADR-008 §9). A recast may end it early. Each ability of this kind is an entry in Abilities.json's
 * selfBuff map.
 */
UCLASS()
class VEYRAABILITIES_API UVeyraSelfBuffAbility : public UVeyraGameplayAbility
{
	GENERATED_BODY()

protected:
	virtual bool Defines(const FVeyraContentId& Ability) const override;
	virtual double GetResourceCost(const FVeyraContentId& Ability, int32 Rank) const override;
	virtual double GetCooldownSeconds(const FVeyraContentId& Ability, int32 Rank) const override;
	virtual const FVeyraCastTuning* GetCastTuning(const FVeyraContentId& Ability) const override;
	virtual bool EndsEarlyOnRecast(const UAbilitySystemComponent& Caster, const FVeyraContentId& Ability) const override;
	virtual void EndEarly(UAbilitySystemComponent& Caster, const FVeyraContentId& Ability) override;
	virtual FVeyraChannelPlan Deliver(const FVeyraCast& Cast) override;

private:
	void RefreshAura();
	void StopAura();
	bool IsAuraRunning() const;

	/** The aura under way: whose, for which ability, and until when in world time. */
	TWeakObjectPtr<UAbilitySystemComponent> AuraCaster;
	FVeyraContentId AuraAbility;
	double AuraEndsAt = 0.0;
	FTimerHandle AuraTimer;
};
