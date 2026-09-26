// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Effects/VeyraResourceSpendExecution.h"

#include "Attributes/VeyraResourceSet.h"
#include "VeyraCombatLog.h"

const FName UVeyraResourceSpendExecution::ResourceCostName(TEXT("Veyra.ResourceCost"));

void UVeyraResourceSpendExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	// A missing magnitude reads as a negative cost, which is refused below.
	constexpr bool bWarnIfMissing = false;
	constexpr float Missing = -1.0f;
	const float Cost = ExecutionParams.GetOwningSpec().GetSetByCallerMagnitude(ResourceCostName, bWarnIfMissing, Missing);
	if (!FMath::IsFinite(Cost) || Cost < 0.0f)
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("A resource cost on %s was not applied: it needs a finite %s of at least 0."),
			*GetNameSafe(ExecutionParams.GetTargetAbilitySystemComponent() ? ExecutionParams.GetTargetAbilitySystemComponent()->GetOwner() : nullptr),
			*ResourceCostName.ToString());
		return;
	}
	OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UVeyraResourceSet::GetResourceSpendAttribute(), EGameplayModOp::AddBase, Cost));
}
