// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Engine/TimerHandle.h"
#include "Passives/VeyraPassive.h"

#include "VeyraUnreturnedPassive.generated.h"

/**
 * Torr's Unreturned (Roster Bible §9; ADR-028 §7): his ordinary regeneration is poor (his data), but out
 * of Vanguard combat his core restores a share of his missing Health each second; and as his Health falls
 * his exposed core grows less encumbered, holding each threshold's statuses below it. Its data is an
 * entry in Vanguards.json's unreturned map.
 */
UCLASS()
class VEYRAVANGUARDS_API UVeyraUnreturnedPassive : public UVeyraPassive
{
	GENERATED_BODY()

public:
	virtual void Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId) override;
	virtual void Stop() override;

	/** Restores and gives the thresholds' statuses; its world timer calls it each check, and tests may. */
	void Check();

private:
	FTimerHandle CheckTimer;
};
