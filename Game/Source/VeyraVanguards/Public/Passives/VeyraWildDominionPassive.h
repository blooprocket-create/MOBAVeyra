// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Engine/TimerHandle.h"
#include "Passives/VeyraPassive.h"

#include "VeyraWildDominionPassive.generated.h"

struct FVeyraDamageResolution;

/**
 * Moro's Wild Dominion (Roster Bible §12; ADR-026 §5): while he stands on jungle terrain, which World
 * derives from the battleground's layout, he holds the passive's statuses, given again at each check,
 * so they lapse soon after he leaves; damage he deals to wildlife restores a share of it as Health.
 * Its data is an entry in Vanguards.json's wildDominion map.
 */
UCLASS()
class VEYRAVANGUARDS_API UVeyraWildDominionPassive : public UVeyraPassive
{
	GENERATED_BODY()

public:
	virtual void Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId) override;
	virtual void Stop() override;

	/** Gives the statuses when its owner stands alive in the jungle. Its world timer calls this each check. */
	void Check();

private:
	void OnDamageResolved(const FVeyraDamageResolution& Resolution);

	FTimerHandle CheckTimer;
	FDelegateHandle ResolvedHandle;
};
