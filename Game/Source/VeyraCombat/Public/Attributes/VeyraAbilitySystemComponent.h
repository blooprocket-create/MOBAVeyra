// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "AbilitySystemComponent.h"

#include "VeyraAbilitySystemComponent.generated.h"

/**
 * A participant's Ability System Component (ADR-016 §3): as the engine's, but its attribute sets —
 * Health, resources and stats — replicate behind the fog, to the participant, its teammates, the
 * replay and those who see its Vanguard (VeyraParticipantData). They are registered so before their
 * first send, as ADR-006 §5's spike found production must.
 */
UCLASS(ClassGroup = Combat)
class VEYRACOMBAT_API UVeyraAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	virtual void ReadyForReplication() override;
};
