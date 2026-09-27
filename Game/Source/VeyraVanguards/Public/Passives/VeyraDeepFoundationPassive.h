// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Passives/VeyraPassive.h"

#include "VeyraDeepFoundationPassive.generated.h"

class AActor;
struct FVeyraAbilityHit;

/**
 * Cairn's Deep Foundation (Character Bible §18): each time one of his abilities immobilizes an enemy
 * Vanguard, by a Stun or a displacement but not a Slow, he gains a shield based on his Max Health.
 * The same Vanguard grants it again only after a lockout, so controls landing together pay once; the
 * shield merges up to its maximum, and its cap group bounds it with his ultimate's shield. A hit whose
 * ability already paid him a shield for that Vanguard grants no passive shield (Burden of the Depths).
 * Its data is an entry in Vanguards.json's deepFoundation map.
 */
UCLASS()
class VEYRAVANGUARDS_API UVeyraDeepFoundationPassive : public UVeyraPassive
{
	GENERATED_BODY()

public:
	virtual void Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId) override;
	virtual void Stop() override;

private:
	void OnAbilityHit(const FVeyraAbilityHit& Hit);

	/** When each enemy Vanguard last granted the shield, in world time. */
	TMap<TWeakObjectPtr<AActor>, double> LastGrantedAt;

	FDelegateHandle HitHandle;
};
