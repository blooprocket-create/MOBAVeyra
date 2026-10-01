// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Passives/VeyraPassive.h"

#include "VeyraAccordPassive.generated.h"

struct FVeyraDamageDealtEvent;

/**
 * Two hunters bound together (ADR-034 §9), as Marek's Bound Together. It summons its owner's companion as it
 * starts. Each of the companion's hits leaves its mark on the enemy; when the owner and the companion have
 * each damaged one enemy within the window, Accord deals that enemy magic damage from the owner, at most once
 * per the enemy's cooldown, stronger while the owner holds the boost status, and shortens one slot's
 * cooldown. Accord's own damage is a proc and starts nothing new. Its data is an entry in Vanguards.json's
 * accord map.
 */
UCLASS()
class VEYRAVANGUARDS_API UVeyraAccordPassive : public UVeyraPassive
{
	GENERATED_BODY()

public:
	virtual void Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId) override;
	virtual void Stop() override;

	/** How many Accords it has dealt. */
	int32 GetAccordCount() const { return AccordCount; }

private:
	void OnDamageDealt(const FVeyraDamageDealtEvent& Event);

	/** When the owner and the companion last damaged one enemy, and when Accord may strike it again, in world time. */
	struct FBound
	{
		TOptional<double> OwnerAt;
		TOptional<double> CompanionAt;
		double ReadyAt = 0.0;
	};
	TMap<TWeakObjectPtr<UAbilitySystemComponent>, FBound> Bound;

	FDelegateHandle DealtHandle;
	bool bStriking = false;
	int32 AccordCount = 0;
};
