// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Passives/VeyraPassive.h"

#include "VeyraSlipstreamPassive.generated.h"

struct FVeyraCastEvent;

/**
 * Aurelisse's Slipstream (Roster Bible §24; ADR-027 §7): each ally-targeted buff she casts at an allied
 * Vanguard leaves a short current from her toward that ally, a lingering rectangle whose statuses speed
 * her and the allied Vanguards inside. Currents are undirected speed for now (ADR-027 §9.4). Its data
 * is an entry in Vanguards.json's slipstream map.
 */
UCLASS()
class VEYRAVANGUARDS_API UVeyraSlipstreamPassive : public UVeyraPassive
{
	GENERATED_BODY()

public:
	virtual void Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId) override;
	virtual void Stop() override;

private:
	void OnCastCommitted(const FVeyraCastEvent& Event);

	FDelegateHandle CastHandle;
};
