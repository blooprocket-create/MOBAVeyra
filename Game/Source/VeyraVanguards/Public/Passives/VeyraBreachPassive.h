// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Passives/VeyraPassive.h"

#include "VeyraBreachPassive.generated.h"

class UVeyraBasicAttackComponent;
struct FVeyraAttackPlan;

/**
 * Bryn's Breach (Character Bible §19). Every third consecutive basic attack on the same enemy
 * Vanguard, at the tuned count, consumes Breach: the attack deals bonus damage and offers one
 * explosion behind its target. An impact of higher priority replaces that explosion, such as Breach
 * Round's, and the bonus is still added once. Neither impact re-enters the hit pipeline (ADR-009 §5).
 * Changing targets starts the count again. Its data is an entry in Vanguards.json's breach map.
 */
UCLASS()
class VEYRAVANGUARDS_API UVeyraBreachPassive : public UVeyraPassive
{
	GENERATED_BODY()

public:
	virtual void Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId) override;
	virtual void Stop() override;

private:
	void OnModifyAttack(FVeyraAttackPlan& Plan);

	TWeakObjectPtr<UVeyraBasicAttackComponent> Attacks;
	FDelegateHandle ModifyHandle;
};
