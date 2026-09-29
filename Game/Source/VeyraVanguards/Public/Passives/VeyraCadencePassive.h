// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Passives/VeyraPassive.h"

#include "VeyraCadencePassive.generated.h"

class UVeyraBasicAttackComponent;
struct FVeyraAttackEvent;
struct FVeyraAttackPlan;
struct FVeyraStatusApplied;

/**
 * Vera's Cadence (Roster Bible §7): basic attacks stack Attack Speed, which decays a stack at a time
 * once she stops (ADR-018 §2); at full Cadence each attack is echoed for part of its damage (Firing
 * Line), and while The Last Volley lasts Cadence is full, cannot fall, and every third attack fires a
 * spectral rank through the target. Its data is an entry in Vanguards.json's cadence map.
 */
UCLASS()
class VEYRAVANGUARDS_API UVeyraCadencePassive : public UVeyraPassive
{
	GENERATED_BODY()

public:
	virtual void Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId) override;
	virtual void Stop() override;

	/** Its stacks now. */
	int32 GetStacks() const;

	/** Whether it is full: at its most stacks, or held full. */
	bool IsFull() const;

private:
	void OnAttack(const FVeyraAttackEvent& Event);
	void OnModifyAttack(FVeyraAttackPlan& Plan);
	void OnStatusApplied(const FVeyraStatusApplied& Event);

	/** Adds Count stacks, lasting at least MinSeconds, at the decay Dig In allows. */
	void AddStacks(int32 Count, double MinSeconds = 0.0);

	/** When the owner's status Id ends, in world time; 0 if it has none. */
	double EndsAt(const FVeyraContentId& Id) const;

	void LaunchEcho(TWeakObjectPtr<AActor> Target, double Amount);
	void FireSpectralRank(AActor& Target);

	TWeakObjectPtr<UVeyraBasicAttackComponent> Attacks;
	FDelegateHandle AttackHandle;
	FDelegateHandle ModifyHandle;
	FDelegateHandle StatusHandle;
	/** Attacks since The Last Volley began. */
	int32 VolleyAttacks = 0;
};
