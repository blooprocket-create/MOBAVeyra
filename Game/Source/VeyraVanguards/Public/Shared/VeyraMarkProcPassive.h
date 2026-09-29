// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Passives/VeyraPassive.h"

#include "VeyraMarkProcPassive.generated.h"

class UVeyraBasicAttackComponent;
struct FVeyraAttackPlan;
struct FVeyraCastEvent;
struct FVeyraStatusApplied;

/**
 * A generic mark-and-proc passive (ADR-008 §5): a basic attack on an enemy Vanguard adds a stack of
 * its owner's mark, or, finding the mark primed at its most stacks, spends it for proc damage and
 * adds none. The first attack out of a Camouflage may be empowered, and for a while after an ability
 * each proc may also send a bolt at a nearby enemy Vanguard its owner could acquire. Mimzi's Pocket
 * Hex is one (Roster Bible §21). Its data is an entry in Vanguards.json's markProc map.
 */
UCLASS()
class VEYRAVANGUARDS_API UVeyraMarkProcPassive : public UVeyraPassive
{
	GENERATED_BODY()

public:
	virtual void Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId) override;
	virtual void Stop() override;

private:
	void OnModifyAttack(FVeyraAttackPlan& Plan);
	void OnStatusApplied(const FVeyraStatusApplied& Event);
	void OnCastCommitted(const FVeyraCastEvent& Event);

	/** Sends a bolt from the owner at the nearest enemy Vanguard it can acquire near Proc's target. */
	void SendBolt(AActor& ProcTarget);

	TWeakObjectPtr<UVeyraBasicAttackComponent> Attacks;
	FDelegateHandle ModifyHandle;
	FDelegateHandle StatusHandle;
	FDelegateHandle CastHandle;
	/** When the last Camouflage ends, in world time, and whether its emergence is spent. */
	double CamouflagedUntil = -1.0;
	bool bEmerged = true;
	/** When the bolt window closes, in world time. */
	double BoltsUntil = -1.0;
};
