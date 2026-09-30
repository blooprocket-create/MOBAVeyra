// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Passives/VeyraPassive.h"

#include "VeyraReclaimPassive.generated.h"

class UVeyraBasicAttackComponent;
struct FVeyraAttackEvent;

/**
 * Silt's Reclaim (Roster Bible §3; ADR-028 §4): his damaging abilities coat enemy Vanguards in Sandy,
 * and each of his basic attacks that lands on a Sandy enemy tears it back into him: it consumes Sandy
 * and heals him, scaling with Magic Power, once per lockout per target. Its data is an entry in
 * Vanguards.json's reclaim map.
 */
UCLASS()
class VEYRAVANGUARDS_API UVeyraReclaimPassive : public UVeyraPassive
{
	GENERATED_BODY()

public:
	virtual void Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId) override;
	virtual void Stop() override;

private:
	void OnHit(const FVeyraAttackEvent& Event);

	TWeakObjectPtr<UVeyraBasicAttackComponent> Attacks;
	FDelegateHandle HitHandle;

	/** When each target was last reclaimed from, in world time. */
	TMap<TWeakObjectPtr<AActor>, double> ReclaimedAt;
};
