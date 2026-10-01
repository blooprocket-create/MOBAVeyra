// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Passives/VeyraPassive.h"
#include "TimerManager.h"

#include "VeyraQuarryPassive.generated.h"

class UVeyraBasicAttackComponent;
struct FVeyraAttackEvent;
struct FVeyraAttackPlan;
struct FVeyraDeathEvent;
struct FVeyraStatusApplied;

/**
 * Tavi's You're It! (Roster Bible §6; ADR-030 §10): her abilities mark one enemy at a time as It. She
 * moves faster while she closes on It; her next basic attack on It spends the mark for bonus magic damage
 * and refunds part of her basic abilities' remaining cooldowns; and when It falls to her, the mark jumps
 * to the nearest enemy Vanguard in reach. Its data is an entry in Vanguards.json's quarry map.
 */
UCLASS()
class VEYRAVANGUARDS_API UVeyraQuarryPassive : public UVeyraPassive
{
	GENERATED_BODY()

public:
	virtual void Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId) override;
	virtual void Stop() override;

	/** The enemy that holds its owner's mark now, or null. */
	AActor* GetQuarry() const;

	/** Server: whether its owner closes on its quarry now, and so holds its chase status. Runs on its timer. */
	void Sample();

private:
	void OnStatusApplied(const FVeyraStatusApplied& Event);
	void OnDeath(const FVeyraDeathEvent& Death);
	void OnModifyAttack(FVeyraAttackPlan& Plan);
	void OnAttackLands(const FVeyraAttackEvent& Event);

	/** Its owner's Level, for the mark's damage. */
	int32 OwnerLevel() const;

	TWeakObjectPtr<UVeyraBasicAttackComponent> Attacks;
	/** The holder of its mark: a participant's or a unit's Ability System Component. */
	TWeakObjectPtr<UAbilitySystemComponent> Quarry;

	/**
	 * Until when, in world time, its quarry counts as It should it fall: as long as its mark lasts, and
	 * after a spending attack only for that attack's own damage, so a lethal spend still sends the mark on.
	 */
	double MarkedUntil = 0.0;
	FDelegateHandle ModifyHandle;
	FDelegateHandle LandingHandle;
	FDelegateHandle StatusHandle;
	FDelegateHandle DeathHandle;
	FTimerHandle SampleTimer;
};
