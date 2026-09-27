// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Passives/VeyraPassive.h"

#include "VeyraHitChainPassive.generated.h"

class UVeyraBasicAttackComponent;
struct FVeyraAttackEvent;

/**
 * A generic passive that builds up over the hit chain, consecutive basic attacks on one enemy
 * Vanguard (ADR-008 §5, ADR-009 §5). Each hit in the chain applies its status again, so a stacking
 * status gains a stack per hit up to its maximum; the chain ending (a switch of target, leaving
 * combat, death) removes it. Qazharr's Sea Dog is one (Character Bible §13). Its data is an entry in
 * Vanguards.json's hitChain map.
 */
UCLASS()
class VEYRAVANGUARDS_API UVeyraHitChainPassive : public UVeyraPassive
{
	GENERATED_BODY()

public:
	virtual void Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId) override;
	virtual void Stop() override;

private:
	void OnHit(const FVeyraAttackEvent& Event);
	void OnChainReset();

	TWeakObjectPtr<UVeyraBasicAttackComponent> Attacks;
	FDelegateHandle HitHandle;
	FDelegateHandle ChainResetHandle;
};
