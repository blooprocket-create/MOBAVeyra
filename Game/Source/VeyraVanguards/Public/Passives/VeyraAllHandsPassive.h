// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Passives/VeyraPassive.h"

#include "VeyraAllHandsPassive.generated.h"

struct FVeyraDamageDealtEvent;

/**
 * Many hands at the line (ADR-037 §6), as Eudora's All Hands. When an allied Vanguard, its owner among them,
 * damages an enemy Vanguard within its radius of its owner's living companion, it earns Work, at most once per
 * its cooldown for each contributor, and never more than its threshold. At the threshold it spends the Work on a
 * repair to the companion's Health. Any delivery counts; the companion's own hits do not. No living companion,
 * no Work. Its data is an entry in Vanguards.json's allHands map.
 */
UCLASS()
class VEYRAVANGUARDS_API UVeyraAllHandsPassive : public UVeyraPassive
{
	GENERATED_BODY()

public:
	virtual void Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId) override;
	virtual void Stop() override;

	/** The Work it holds now. */
	double GetWork() const { return Work; }

	/** How many repairs it has made. */
	int32 GetRepairCount() const { return RepairCount; }

private:
	void OnDamageDealt(const FVeyraDamageDealtEvent& Event);

	/** When each contributor may earn Work again, in world time. */
	TMap<TWeakObjectPtr<const UAbilitySystemComponent>, double> ReadyAt;
	FDelegateHandle DealtHandle;
	double Work = 0.0;
	int32 RepairCount = 0;
};
