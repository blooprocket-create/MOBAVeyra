// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "AbilitySystemComponent.h"
#include "AttributeSet.h"

#include "VeyraOffenceSet.generated.h"

/**
 * What an attacker brings to a damage event (Combat Bible §3, §15). The damage execution snapshots
 * these when the damage is created. Percentage stats are stored as the fraction that remains or the
 * factor applied, so several sources combine as a product (§41, author ruling 2026-09-25).
 */
UCLASS()
class VEYRACOMBAT_API UVeyraOffenceSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UVeyraOffenceSet();

	ATTRIBUTE_ACCESSORS_BASIC(UVeyraOffenceSet, OutgoingDamageMultiplier)
	ATTRIBUTE_ACCESSORS_BASIC(UVeyraOffenceSet, PhysicalPenetrationFlat)
	ATTRIBUTE_ACCESSORS_BASIC(UVeyraOffenceSet, PhysicalPenetrationRetained)
	ATTRIBUTE_ACCESSORS_BASIC(UVeyraOffenceSet, MagicPenetrationFlat)
	ATTRIBUTE_ACCESSORS_BASIC(UVeyraOffenceSet, MagicPenetrationRetained)

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_OutgoingDamageMultiplier(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_PhysicalPenetrationFlat(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_PhysicalPenetrationRetained(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MagicPenetrationFlat(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MagicPenetrationRetained(const FGameplayAttributeData& OldValue);

private:
	/** Generic Damage Amplification (§15): the product of every source's 1 + x. */
	UPROPERTY(ReplicatedUsing = OnRep_OutgoingDamageMultiplier)
	FGameplayAttributeData OutgoingDamageMultiplier;

	/** Flat Armor penetration. */
	UPROPERTY(ReplicatedUsing = OnRep_PhysicalPenetrationFlat)
	FGameplayAttributeData PhysicalPenetrationFlat;

	/** The fraction of Armor left after percentage penetration: the product of every source's 1 − x. */
	UPROPERTY(ReplicatedUsing = OnRep_PhysicalPenetrationRetained)
	FGameplayAttributeData PhysicalPenetrationRetained;

	/** Flat Magic Resistance penetration. */
	UPROPERTY(ReplicatedUsing = OnRep_MagicPenetrationFlat)
	FGameplayAttributeData MagicPenetrationFlat;

	/** The fraction of Magic Resistance left after percentage penetration. */
	UPROPERTY(ReplicatedUsing = OnRep_MagicPenetrationRetained)
	FGameplayAttributeData MagicPenetrationRetained;
};
