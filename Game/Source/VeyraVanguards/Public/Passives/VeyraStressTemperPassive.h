// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Passives/VeyraPassive.h"

#include "VeyraStressTemperPassive.generated.h"

struct FVeyraAbilityHit;
struct FVeyraUnitMovedEvent;

/**
 * Movement punished (ADR-032 §2), as Varkesh's Stress Temper: each hit of its owner's damaging abilities on
 * an enemy Vanguard coats it; a coated unit that dashes or blinks on its own, and holds no lockout from the
 * owner, is struck where it lands. The coating goes, the unit takes the lockout and the strike's statuses,
 * and the strike's damage lands. Its data is an entry in Vanguards.json's stressTemper map.
 */
UCLASS()
class VEYRAVANGUARDS_API UVeyraStressTemperPassive : public UVeyraPassive
{
	GENERATED_BODY()

public:
	virtual void Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId) override;
	virtual void Stop() override;

private:
	void OnAbilityHit(const FVeyraAbilityHit& Hit);
	void OnUnitMoved(const FVeyraUnitMovedEvent& Moved);

	FDelegateHandle HitHandle;
	FDelegateHandle MovedHandle;
};
