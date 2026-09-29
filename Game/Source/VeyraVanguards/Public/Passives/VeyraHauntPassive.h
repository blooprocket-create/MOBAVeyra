// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Passives/VeyraPassive.h"

#include "VeyraHauntPassive.generated.h"

struct FVeyraHostileDamageEvent;

/**
 * Patch's Haunted Attachment (Roster Bible §5): an enemy Vanguard that damages Patch is Haunted, a
 * status from him, once per its own cooldown. A Haunted enemy that damages one of his allied Vanguards
 * near him instead is lashed by the spirit, proc damage and statuses such as a brief Slow, and the
 * Haunt is spent. Its data is an entry in Vanguards.json's haunt map.
 */
UCLASS()
class VEYRAVANGUARDS_API UVeyraHauntPassive : public UVeyraPassive
{
	GENERATED_BODY()

public:
	virtual void Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId) override;
	virtual void Stop() override;

private:
	void OnHostileDamage(const FVeyraHostileDamageEvent& Event);

	/** When each enemy can next be Haunted, in world seconds. */
	TMap<TWeakObjectPtr<UAbilitySystemComponent>, double> NextHauntAt;
	FDelegateHandle DamageHandle;
};
