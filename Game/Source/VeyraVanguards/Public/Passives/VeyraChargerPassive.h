// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Passives/VeyraPassive.h"

#include "VeyraChargerPassive.generated.h"

struct FVeyraDeathEvent;

/**
 * A battery fed by nearby battles (ADR-033 §2), as Relay's Charger: each Fluxborn that dies within its
 * radius of its living owner, of either side, gives Charge, and more while its owner holds the boost status
 * Overcharge gives. Wildlife, Vanguards and structures give none. Its data is an entry in
 * Vanguards.json's charger map.
 */
UCLASS()
class VEYRAVANGUARDS_API UVeyraChargerPassive : public UVeyraPassive
{
	GENERATED_BODY()

public:
	virtual void Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId) override;
	virtual void Stop() override;

private:
	void OnDeath(const FVeyraDeathEvent& Death);

	FDelegateHandle DeathHandle;
};
