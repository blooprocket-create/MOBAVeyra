// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Passives/VeyraPassive.h"

#include "VeyraAttackStridePassive.generated.h"

class UVeyraBasicAttackComponent;
struct FVeyraAttackEvent;

/**
 * Celandrine's Never Break Stride (Roster Bible §22; ADR-027 §1, §8): she walks through her basic
 * attacks' windups at a share of her speed instead of stopping, and each primary basic attack that lands
 * on an enemy Vanguard gives her the passive's statuses, a speed burst refreshed rather than stacked.
 * Wildlife, Fluxborn and structures give nothing. Its data is an entry in Vanguards.json's attackStride map.
 */
UCLASS()
class VEYRAVANGUARDS_API UVeyraAttackStridePassive : public UVeyraPassive
{
	GENERATED_BODY()

public:
	virtual void Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId) override;
	virtual void Stop() override;

private:
	void OnHit(const FVeyraAttackEvent& Event);

	TWeakObjectPtr<UVeyraBasicAttackComponent> Attacks;
	FDelegateHandle HitHandle;
};
