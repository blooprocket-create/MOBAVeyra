// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Passives/VeyraPassive.h"

#include "VeyraDisciplinesPassive.generated.h"

class AActor;
struct FVeyraAbilityHit;
struct FVeyraDisciplineMarkTuning;

/**
 * Two disciplines whose abilities mark enemy Vanguards for the other to spend (ADR-031 §10), as Angeru's
 * No Master: when one of a mark's spending abilities hits an enemy Vanguard that holds the mark from the
 * passive's owner, the mark goes, an extra strike lands, part of the ability's cost comes back, the
 * owner takes the mark's statuses, the ability's own bonus applies, and a named slot's remaining
 * cooldown shortens. Its data is an entry in Vanguards.json's disciplines map.
 */
UCLASS()
class VEYRAVANGUARDS_API UVeyraDisciplinesPassive : public UVeyraPassive
{
	GENERATED_BODY()

public:
	virtual void Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId) override;
	virtual void Stop() override;

private:
	void OnAbilityHit(const FVeyraAbilityHit& Hit);

	/** The extra strike as Mark is spent on Target, Multiplier times as hard. */
	static void Strike(UAbilitySystemComponent& Owner, UAbilitySystemComponent& Target, const FVeyraDisciplineMarkTuning& Mark, double Multiplier);

	FDelegateHandle HitHandle;
};
