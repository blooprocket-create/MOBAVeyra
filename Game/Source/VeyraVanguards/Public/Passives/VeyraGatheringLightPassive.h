// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Passives/VeyraPassive.h"

#include "VeyraGatheringLightPassive.generated.h"

class AActor;
struct FVeyraAbilityHit;
struct FVeyraDeathEvent;

/**
 * Oriel's Gathering Light (Character Bible §20):
 * - Each damaging ability cast that hits an enemy Vanguard adds one stack, however many it hits.
 * - At the tuned count the passive is primed. The next such cast consumes the stacks and sends a
 *   homing glass fragment at one struck enemy Vanguard she can acquire (VeyraTargeting::CanAcquire).
 * - A primed cast that hits no Vanguard she can acquire leaves it primed, with no fragment.
 * - The consuming cast adds no stack, and death clears the stacks (ADR-008 §9).
 * Its data is an entry in Vanguards.json's gatheringLight map.
 */
UCLASS()
class VEYRAVANGUARDS_API UVeyraGatheringLightPassive : public UVeyraPassive
{
	GENERATED_BODY()

public:
	virtual void Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId) override;
	virtual void Stop() override;

	int32 GetStacks() const { return Stacks; }

	/** Whether the next damaging cast that hits an enemy Vanguard she can acquire sends a fragment. */
	bool IsPrimed() const;

private:
	void OnAbilityHit(const FVeyraAbilityHit& Hit);
	void OnDeath(const FVeyraDeathEvent& Death);
	void LaunchFragment(UAbilitySystemComponent& Owner, AActor& Target, int32 CastId);

	int32 Stacks = 0;

	/** The casts that already counted, so each counts once however many units it hits and however late it lands. */
	TSet<int32> CountedCasts;

	FDelegateHandle HitHandle;
	FDelegateHandle DeathHandle;
};
