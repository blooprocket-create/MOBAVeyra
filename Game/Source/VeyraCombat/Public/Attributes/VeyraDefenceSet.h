// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "AbilitySystemComponent.h"
#include "AttributeSet.h"

#include "VeyraDefenceSet.generated.h"

/**
 * What a defender brings to a damage event (Combat Bible §3, §15). Armor and Magic Resistance are §41
 * stats; resistance reduction is kept separate so the §3 order (flat, then percentage) holds, and
 * VeyraDamage::ReduceResistance gives the reduced stat.
 */
UCLASS()
class VEYRACOMBAT_API UVeyraDefenceSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UVeyraDefenceSet();

	ATTRIBUTE_ACCESSORS_BASIC(UVeyraDefenceSet, Armor)
	ATTRIBUTE_ACCESSORS_BASIC(UVeyraDefenceSet, MagicResist)
	ATTRIBUTE_ACCESSORS_BASIC(UVeyraDefenceSet, ArmorReductionFlat)
	ATTRIBUTE_ACCESSORS_BASIC(UVeyraDefenceSet, ArmorReductionRetained)
	ATTRIBUTE_ACCESSORS_BASIC(UVeyraDefenceSet, MagicResistReductionFlat)
	ATTRIBUTE_ACCESSORS_BASIC(UVeyraDefenceSet, MagicResistReductionRetained)
	ATTRIBUTE_ACCESSORS_BASIC(UVeyraDefenceSet, IncomingDamageMultiplier)
	ATTRIBUTE_ACCESSORS_BASIC(UVeyraDefenceSet, TenacityRetained)
	ATTRIBUTE_ACCESSORS_BASIC(UVeyraDefenceSet, DisplacementRetained)

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_Armor(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MagicResist(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_ArmorReductionFlat(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_ArmorReductionRetained(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MagicResistReductionFlat(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MagicResistReductionRetained(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_IncomingDamageMultiplier(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_TenacityRetained(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_DisplacementRetained(const FGameplayAttributeData& OldValue);

private:
	UPROPERTY(ReplicatedUsing = OnRep_Armor)
	FGameplayAttributeData Armor;

	UPROPERTY(ReplicatedUsing = OnRep_MagicResist)
	FGameplayAttributeData MagicResist;

	/** Flat Armor reduction; it may push Armor below 0. */
	UPROPERTY(ReplicatedUsing = OnRep_ArmorReductionFlat)
	FGameplayAttributeData ArmorReductionFlat;

	/** The fraction of positive Armor left after percentage reduction: the product of every source's 1 − x. */
	UPROPERTY(ReplicatedUsing = OnRep_ArmorReductionRetained)
	FGameplayAttributeData ArmorReductionRetained;

	UPROPERTY(ReplicatedUsing = OnRep_MagicResistReductionFlat)
	FGameplayAttributeData MagicResistReductionFlat;

	UPROPERTY(ReplicatedUsing = OnRep_MagicResistReductionRetained)
	FGameplayAttributeData MagicResistReductionRetained;

	/** Generic Damage Reduction (§15): the product of every source's 1 − x. */
	UPROPERTY(ReplicatedUsing = OnRep_IncomingDamageMultiplier)
	FGameplayAttributeData IncomingDamageMultiplier;

	/**
	 * The fraction of a Tenacity-reducible crowd-control duration that remains (Combat Bible §8):
	 * the product of every Tenacity source's 1 − x, since "Tenacity sources stack multiplicatively".
	 */
	UPROPERTY(ReplicatedUsing = OnRep_TenacityRetained)
	FGameplayAttributeData TenacityRetained;

	/**
	 * The fraction of a forced movement's distance that remains (Combat Bible §9, Displacement
	 * Resistance): the product of every source's 1 − x. Airborne time is unchanged.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_DisplacementRetained)
	FGameplayAttributeData DisplacementRetained;
};
