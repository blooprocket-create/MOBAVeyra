// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"

#include "VeyraTargetDataSpike.generated.h"

// Types for the ADR-006 §5 target-data spike (Veyra.Net.TargetData). Veyra's own abilities send no
// client target data (ADR-006 §7); these exist only to learn how GAS target data behaves under Iris
// before an ability kind needs it. UHT forbids preprocessor guards around them.

/** Target data built from plain properties, which Iris serializes from the struct's reflection. */
USTRUCT()
struct FVeyraSpikePropertyTargetData : public FGameplayAbilityTargetData
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize100 Point = FVector_NetQuantize100(FVector::ZeroVector);

	UPROPERTY()
	int32 Marker = 0;

	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
};

/**
 * A locally predicted ability that does nothing. The spike grants it for a valid ability handle and
 * tries to activate it from the owning client.
 */
UCLASS()
class UVeyraTargetDataSpikeAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UVeyraTargetDataSpikeAbility();
};
