// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Abilities/VeyraGameplayAbility.h"
#include "Engine/TimerHandle.h"

#include "VeyraSelfBuffAbility.generated.h"

struct FVeyraHealTuning;

/**
 * The archetype for an ability that buffs its caster (ADR-008 §3): statuses and a shield on the
 * caster, optionally an aura of statuses for nearby allied Vanguards, refreshed on an interval
 * (ADR-008 §9), and optionally a heal for the caster and its most wounded ally (ADR-015 §3). A recast
 * may end it early. Each ability of this kind is an entry in Abilities.json's selfBuff map.
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
	virtual bool IsOffensive(const FVeyraContentId& Ability) const override;

private:
	/**
	 * The forms of Ability's stance: its slot's own ability and the override that holds the slot, each
	 * a self-buff its recast ends early (ADR-018 §1), as Vera's Dig In and its volley form. They share
	 * one stance: casting one ends the others', and recasting any ends them all. Only Ability itself
	 * for any other.
	 */
	TArray<FVeyraContentId> FormsOf(const UAbilitySystemComponent& Caster, const FVeyraContentId& Ability) const;

	/** Removes the statuses of each of Forms. */
	static void EndForms(UAbilitySystemComponent& Caster, TConstArrayView<FVeyraContentId> Forms);

	void RefreshAura();
	void StopAura();

	/** Restores Heal's Health to Caster and to its most wounded ally in range, and gives each Heal's statuses (ADR-015 §3). */
	void DeliverHeal(UAbilitySystemComponent& Caster, const FVeyraHealTuning& Heal) const;

	/** The living allied Vanguard within Range of Caster that lacks the most of its Health; none if all are whole. */
	UAbilitySystemComponent* FindMostWoundedAlly(const UAbilitySystemComponent& Caster, double Range) const;
	bool IsAuraRunning() const;

	/** The aura under way: whose, for which ability, and until when in world time. */
	TWeakObjectPtr<UAbilitySystemComponent> AuraCaster;
	FVeyraContentId AuraAbility;
	double AuraEndsAt = 0.0;
	FTimerHandle AuraTimer;
};
