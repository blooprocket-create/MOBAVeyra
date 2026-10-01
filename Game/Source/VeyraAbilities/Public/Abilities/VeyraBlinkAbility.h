// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Abilities/VeyraGameplayAbility.h"

#include "VeyraBlinkAbility.generated.h"

class AVeyraPlacedMarker;
struct FVeyraBlinkAbilityTuning;

/**
 * The blink archetype (ADR-031 §5), as Angeru's Black Step and False Body's swap: it moves its caster at
 * once, beside an enemy unit it then strikes, or to its caster's own standing marker, which may take
 * the caster's old place. Each ability of this kind is an entry in Abilities.json's blink map. It moves
 * its caster, so Root and Grounded refuse it; during its caster's dash it takes over.
 */
UCLASS()
class VEYRAABILITIES_API UVeyraBlinkAbility : public UVeyraGameplayAbility
{
	GENERATED_BODY()

protected:
	virtual bool Defines(const FVeyraContentId& Ability) const override;
	virtual double GetResourceCost(const FVeyraContentId& Ability, int32 Rank) const override;
	virtual double GetCooldownSeconds(const FVeyraContentId& Ability, int32 Rank) const override;
	virtual EVeyraCastRejection CheckTarget(const AActor& Caster, const FVeyraContentId& Ability, const FVeyraCastTarget& Target) const override;
	virtual const FVeyraCastTuning* GetCastTuning(const FVeyraContentId& Ability) const override;
	virtual bool MovesCaster(const FVeyraContentId& Ability) const override { return Defines(Ability); }
	/** Its blink ends a dash under way (ADR-031 §7). */
	virtual bool TakesOverDash(const FVeyraContentId& Ability) const override { return Defines(Ability); }
	virtual FVeyraChannelPlan Deliver(const FVeyraCast& Cast) override;
	virtual bool IsOffensive(const FVeyraContentId& Ability) const override;

private:
	/** Caster's standing marker that Blink may go to, if it names one. */
	static AVeyraPlacedMarker* OwnMarkerOf(const UAbilitySystemComponent& Caster, const FVeyraBlinkAbilityTuning& Blink);
};
