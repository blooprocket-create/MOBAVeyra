// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Abilities/VeyraGameplayAbility.h"

#include "VeyraAmbushAbility.generated.h"

/**
 * The ambush archetype (ADR-030 §9), as Tavi's Ready or Not!: cast on an enemy Vanguard its caster
 * damaged, crowd-controlled or debuffed lately, the caster vanishes, Invisible and Untargetable, then
 * blinks beside its target and strikes. Each ability of this kind is an entry in Abilities.json's
 * ambush map. Its blink moves its caster, so Root and Grounded refuse the cast.
 */
UCLASS()
class VEYRAABILITIES_API UVeyraAmbushAbility : public UVeyraGameplayAbility
{
	GENERATED_BODY()

public:
	/** Whether Caster damaged, crowd-controlled or debuffed Target within Seconds of now (ADR-030 §9). */
	static bool HurtLately(const UAbilitySystemComponent& Caster, const AActor& Target, double Seconds);

protected:
	virtual bool Defines(const FVeyraContentId& Ability) const override;
	virtual double GetResourceCost(const FVeyraContentId& Ability, int32 Rank) const override;
	virtual double GetCooldownSeconds(const FVeyraContentId& Ability, int32 Rank) const override;
	virtual EVeyraCastRejection CheckTarget(const AActor& Caster, const FVeyraContentId& Ability, const FVeyraCastTarget& Target) const override;
	virtual const FVeyraCastTuning* GetCastTuning(const FVeyraContentId& Ability) const override;
	virtual bool MovesCaster(const FVeyraContentId& Ability) const override { return Defines(Ability); }
	virtual FVeyraChannelPlan Deliver(const FVeyraCast& Cast) override;
};
