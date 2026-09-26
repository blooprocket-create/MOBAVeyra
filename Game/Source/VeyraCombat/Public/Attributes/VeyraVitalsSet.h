// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "Damage/VeyraDamageTypes.h"
#include "Misc/Optional.h"

#include "VeyraVitalsSet.generated.h"

/**
 * Health and the damage pipeline's entry points (ADR-006 §4). Health is a pool: only this set writes
 * it, from the Incoming*Damage meta attributes the damage execution outputs, after shields and
 * Temporary Health have had their turn (Combat Bible §25 steps 7–9).
 */
UCLASS()
class VEYRACOMBAT_API UVeyraVitalsSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	ATTRIBUTE_ACCESSORS_BASIC(UVeyraVitalsSet, Health)
	ATTRIBUTE_ACCESSORS_BASIC(UVeyraVitalsSet, MaxHealth)
	ATTRIBUTE_ACCESSORS_BASIC(UVeyraVitalsSet, IncomingPhysicalDamage)
	ATTRIBUTE_ACCESSORS_BASIC(UVeyraVitalsSet, IncomingMagicDamage)
	ATTRIBUTE_ACCESSORS_BASIC(UVeyraVitalsSet, IncomingTrueDamage)

	/** The meta attribute a damage type's mitigated damage arrives in. */
	static FGameplayAttribute GetIncomingDamageAttribute(EVeyraDamageType Type);

	/** The damage type an Incoming*Damage meta attribute carries, if Attribute is one. */
	static TOptional<EVeyraDamageType> GetIncomingDamageType(const FGameplayAttribute& Attribute);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual bool PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

private:
	/** Ordinary Health, never below 0 or above MaxHealth (Combat Bible §39). */
	UPROPERTY(ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;

	/** A §41 stat. A change keeps the Health percentage (§41), and it must stay above 0 (§39). */
	UPROPERTY(ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;

	/** Meta attributes: mitigated damage of each type on its way to absorption. Never replicated. */
	UPROPERTY(meta = (HideFromModifiers))
	FGameplayAttributeData IncomingPhysicalDamage;

	UPROPERTY(meta = (HideFromModifiers))
	FGameplayAttributeData IncomingMagicDamage;

	UPROPERTY(meta = (HideFromModifiers))
	FGameplayAttributeData IncomingTrueDamage;
};
