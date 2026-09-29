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
	ATTRIBUTE_ACCESSORS_BASIC(UVeyraOffenceSet, PhysicalPower)
	ATTRIBUTE_ACCESSORS_BASIC(UVeyraOffenceSet, MagicPower)
	ATTRIBUTE_ACCESSORS_BASIC(UVeyraOffenceSet, AttackSpeed)
	ATTRIBUTE_ACCESSORS_BASIC(UVeyraOffenceSet, AbilityHaste)

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;

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

	UFUNCTION()
	void OnRep_PhysicalPower(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MagicPower(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_AttackSpeed(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_AbilityHaste(const FGameplayAttributeData& OldValue);

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

	/** A §41 stat that physical ratios scale with (Combat Bible §3). */
	UPROPERTY(ReplicatedUsing = OnRep_PhysicalPower)
	FGameplayAttributeData PhysicalPower;

	/** A §41 stat that magic ratios scale with (Combat Bible §3). */
	UPROPERTY(ReplicatedUsing = OnRep_MagicPower)
	FGameplayAttributeData MagicPower;

	/**
	 * Attacks per second before the Combat Bible §22 cap and minimum, which the basic attack applies
	 * with the overflow rule. A §41 stat.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_AttackSpeed)
	FGameplayAttributeData AttackSpeed;

	/**
	 * Shortens ability cooldowns to 100 / (100 + Ability Haste) of their length (Combat Bible §21); read
	 * floored at 0 (§39). A §41 stat.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_AbilityHaste)
	FGameplayAttributeData AbilityHaste;
};
