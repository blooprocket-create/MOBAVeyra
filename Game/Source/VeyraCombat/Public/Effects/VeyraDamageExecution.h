// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "GameplayEffectExecutionCalculation.h"

#include "VeyraDamageExecution.generated.h"

/**
 * The Gameplay Ability System's entry into the canonical damage pipeline (ADR-002). It holds no
 * formula: it reads the damage from the spec, captures the attacker's offence (snapshotted) and the
 * defender's defence, calls VeyraDamage for §25 steps 2–6 and outputs the mitigated damage to the
 * defender's Incoming*Damage meta attributes, where UVeyraVitalsSet finishes steps 7–9.
 */
UCLASS()
class VEYRACOMBAT_API UVeyraDamageExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UVeyraDamageExecution();

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
