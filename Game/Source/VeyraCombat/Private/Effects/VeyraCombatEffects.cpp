// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Effects/VeyraCombatEffects.h"

#include "Attributes/VeyraDefenceSet.h"
#include "Attributes/VeyraMobilitySet.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Effects/VeyraDamageExecution.h"
#include "Effects/VeyraResourceSpendExecution.h"

const FName UVeyraDamageEffect::PhysicalPenetrationFlatName(TEXT("PhysicalPenetrationFlat"));
const FName UVeyraDamageEffect::PhysicalPenetrationRetainedName(TEXT("PhysicalPenetrationRetained"));
const FName UVeyraDamageEffect::MagicPenetrationFlatName(TEXT("MagicPenetrationFlat"));
const FName UVeyraDamageEffect::MagicPenetrationRetainedName(TEXT("MagicPenetrationRetained"));

const FName UVeyraStatusEffect::MoveSpeedMultiplierName(TEXT("MoveSpeedMultiplier"));
const FName UVeyraStatusEffect::AttackSpeedMultiplierName(TEXT("AttackSpeedMultiplier"));
const FName UVeyraStatusEffect::TenacityMultiplierName(TEXT("TenacityMultiplier"));
const FName UVeyraStatusEffect::IncomingDamageMultiplierName(TEXT("IncomingDamageMultiplier"));
const FName UVeyraStatusEffect::DisplacementMultiplierName(TEXT("DisplacementMultiplier"));

UVeyraStatusEffect::UVeyraStatusEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	const TPair<FGameplayAttribute, FName> Lines[] = {
		{ UVeyraMobilitySet::GetMoveSpeedAttribute(), MoveSpeedMultiplierName },
		{ UVeyraOffenceSet::GetAttackSpeedAttribute(), AttackSpeedMultiplierName },
		{ UVeyraDefenceSet::GetTenacityRetainedAttribute(), TenacityMultiplierName },
		{ UVeyraDefenceSet::GetIncomingDamageMultiplierAttribute(), IncomingDamageMultiplierName },
		{ UVeyraDefenceSet::GetDisplacementRetainedAttribute(), DisplacementMultiplierName },
	};
	for (const TPair<FGameplayAttribute, FName>& Line : Lines)
	{
		FSetByCallerFloat Multiplier;
		Multiplier.DataName = Line.Value;
		FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Line.Key;
		Modifier.ModifierOp = EGameplayModOp::MultiplyCompound;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Multiplier);
	}
}

UVeyraStatusMarkerEffect::UVeyraStatusMarkerEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
}

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

UVeyraResourceSpendEffect::UVeyraResourceSpendEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayEffectExecutionDefinition Execution;
	Execution.CalculationClass = UVeyraResourceSpendExecution::StaticClass();
	Executions.Add(Execution);
}
