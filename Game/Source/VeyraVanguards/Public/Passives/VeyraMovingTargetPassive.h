// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Passives/VeyraPassive.h"

#include "VeyraMovingTargetPassive.generated.h"

class UVeyraBasicAttackComponent;
struct FVeyraAttackPlan;
struct FVeyraDisplacementEvent;

/**
 * Kade's Moving Target (Roster Bible §2): enemy Vanguards that Kade or his allies displace become
 * Tracked, a status from him that lengthens his range against them; his attacks on a Tracked target
 * deal bonus damage, and displacement banks toward Dead Reckoning, which the next such attack spends.
 * Its data is an entry in Vanguards.json's movingTarget map.
 */
UCLASS()
class VEYRAVANGUARDS_API UVeyraMovingTargetPassive : public UVeyraPassive
{
	GENERATED_BODY()

public:
	virtual void Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId) override;
	virtual void Stop() override;

	/** Displacement banked toward Dead Reckoning, in units. */
	double GetBanked() const { return Banked; }

private:
	void OnDisplaced(const FVeyraDisplacementEvent& Event);
	void OnModifyAttack(FVeyraAttackPlan& Plan);

	TWeakObjectPtr<UVeyraBasicAttackComponent> Attacks;
	FDelegateHandle ModifyHandle;
	FDelegateHandle DisplacedHandle;
	double Banked = 0.0;
};
