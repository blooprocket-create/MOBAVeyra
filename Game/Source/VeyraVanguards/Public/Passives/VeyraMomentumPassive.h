// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Engine/TimerHandle.h"
#include "Passives/VeyraPassive.h"

#include "VeyraMomentumPassive.generated.h"

class UVeyraBasicAttackComponent;
struct FVeyraAttackEvent;
struct FVeyraAttackPlan;
struct FVeyraCastEvent;
struct FVeyraDeathEvent;

/**
 * Raska's Redline (Roster Bible §1): Momentum builds with the distance she moves herself, never with
 * forced movement, and with her attacks and casts; a stationary Raska builds none, mounted or not. At
 * full, her basic abilities' slots hold their Redlined forms until one is cast, which spends the meter
 * and readies Roadhouse: her next basic attack on an enemy Vanguard lunges with more reach and deals
 * bonus damage from the target's missing Health and her bonus Health. A ride holds Redline off, since
 * its mounted set has no Redlined forms, and keeps Roadhouse waiting, since a rider cannot attack.
 * Its data is an entry in Vanguards.json's momentum map.
 */
UCLASS()
class VEYRAVANGUARDS_API UVeyraMomentumPassive : public UVeyraPassive
{
	GENERATED_BODY()

public:
	virtual void Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId) override;
	virtual void Stop() override;

	/** Momentum now: the meter's stacks. */
	int32 GetMomentum() const;

	/** Whether her slots hold their Redlined forms. */
	bool IsRedlined() const { return bRedlined; }

	/** Whether her next basic attack on an enemy Vanguard is Roadhouse. */
	bool IsRoadhouseReady() const { return bRoadhouse; }

	/** Counts the distance she has moved herself since the last count; its timer calls it, and tests may. */
	void Sample();

private:
	void OnHit(const FVeyraAttackEvent& Event);
	void OnCastCommitted(const FVeyraCastEvent& Event);
	void OnModifyAttack(FVeyraAttackPlan& Plan);
	void OnDeath(const FVeyraDeathEvent& Death);

	/** Adds Points to the meter, then readies Redline if it is full. */
	void AddPoints(int32 Points);

	/** The meter's full: its status's most stacks. */
	int32 GetMost() const;
	bool IsHeldFull() const;

	/** At full, and off the ride, its slots take their Redlined forms. */
	void MaybeRedline();

	FName RedlineGroup() const;

	TWeakObjectPtr<UVeyraBasicAttackComponent> Attacks;
	FDelegateHandle HitHandle;
	FDelegateHandle ModifyHandle;
	FDelegateHandle CastHandle;
	FDelegateHandle DeathHandle;
	FTimerHandle SampleTimer;
	/** Distance counted toward the next point. */
	double Carried = 0.0;
	bool bRedlined = false;
	bool bRoadhouse = false;
};
