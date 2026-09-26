// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "GameplayEffectExecutionCalculation.h"

#include "VeyraResourceSpendExecution.generated.h"

/**
 * Takes a cost from the target's Resource (Combat Bible §27). It reads the amount from the spec's
 * SetByCaller magnitude named ResourceCostName and outputs it to the ResourceSpend meta attribute,
 * which UVeyraResourceSet applies. VeyraCombat::SpendResource checks the amount is affordable first.
 */
UCLASS()
class VEYRACOMBAT_API UVeyraResourceSpendExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	/** The SetByCaller name that carries the cost. An internal key, not a Gameplay Tag. */
	static const FName ResourceCostName;

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
