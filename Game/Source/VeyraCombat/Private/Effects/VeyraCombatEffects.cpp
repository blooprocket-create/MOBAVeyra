// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Effects/VeyraCombatEffects.h"

#include "Effects/VeyraDamageExecution.h"

UVeyraDamageEffect::UVeyraDamageEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayEffectExecutionDefinition Execution;
	Execution.CalculationClass = UVeyraDamageExecution::StaticClass();
	Executions.Add(Execution);
}

UVeyraShieldEffect::UVeyraShieldEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
}

UVeyraTemporaryHealthEffect::UVeyraTemporaryHealthEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
}
