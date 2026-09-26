// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "AbilitySystemComponent.h"
#include "AttributeSet.h"

#include "VeyraMobilitySet.generated.h"

/**
 * How a unit moves (Combat Bible §23). Move Speed is a §41 stat; the pawn's movement component
 * follows it on the server. The §23 soft caps and the slow floor arrive with the first effect that
 * changes speed.
 */
UCLASS()
class VEYRACOMBAT_API UVeyraMobilitySet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UVeyraMobilitySet();

	ATTRIBUTE_ACCESSORS_BASIC(UVeyraMobilitySet, MoveSpeed)

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_MoveSpeed(const FGameplayAttributeData& OldValue);

private:
	/** Units per second. */
	UPROPERTY(ReplicatedUsing = OnRep_MoveSpeed)
	FGameplayAttributeData MoveSpeed;
};
