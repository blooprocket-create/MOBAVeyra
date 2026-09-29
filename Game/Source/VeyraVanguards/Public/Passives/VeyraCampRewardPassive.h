// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Passives/VeyraPassive.h"

#include "VeyraCampRewardPassive.generated.h"

struct FVeyraCampCleared;
struct FVeyraDeathEvent;

/**
 * Gorraveth's No Time to Bleed (Roster Bible §23): helping clear a whole jungle camp restores Health
 * and gives statuses such as a burst of Movement Speed, once per cleared camp and only while he lives;
 * an enemy Vanguard takedown, his kill or his assist, gives the same on its own cooldown. Its data is
 * an entry in Vanguards.json's campReward map.
 */
UCLASS()
class VEYRAVANGUARDS_API UVeyraCampRewardPassive : public UVeyraPassive
{
	GENERATED_BODY()

public:
	virtual void Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId) override;
	virtual void Stop() override;

private:
	void OnCampCleared(const FVeyraCampCleared& Cleared);
	void OnDeath(const FVeyraDeathEvent& Death);

	/** Restores his Health and gives the statuses, if he lives. */
	void Reward();

	/** When a takedown can next reward him, in world seconds. */
	double NextTakedownAt = 0.0;
	FDelegateHandle CampHandle;
	FDelegateHandle DeathHandle;
};
