// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "AbilitySystemComponent.h"
#include "AttributeSet.h"

#include "VeyraResourceSet.generated.h"

/**
 * The resource abilities spend (Combat Bible §27). Which resource family a Vanguard uses (the
 * standard resource, Mana; Focus; Charge; or none) is data, not a class per Vanguard (ADR-006 §4);
 * M3 backs the standard family only. A resource-less Vanguard has a Max Resource of 0.
 *
 * Resource is a pool like Health: only this set writes it, and spending arrives through the
 * ResourceSpend meta attribute, which VeyraCombat::SpendResource fills.
 */
UCLASS()
class VEYRACOMBAT_API UVeyraResourceSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	ATTRIBUTE_ACCESSORS_BASIC(UVeyraResourceSet, Resource)
	ATTRIBUTE_ACCESSORS_BASIC(UVeyraResourceSet, MaxResource)
	ATTRIBUTE_ACCESSORS_BASIC(UVeyraResourceSet, ResourceSpend)

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual bool PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

protected:
	UFUNCTION()
	void OnRep_Resource(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxResource(const FGameplayAttributeData& OldValue);

private:
	/** Never below 0 or above Max Resource (Combat Bible §27: resources do not go negative). */
	UPROPERTY(ReplicatedUsing = OnRep_Resource)
	FGameplayAttributeData Resource;

	/** A §41 stat; 0 for a resource-less Vanguard. */
	UPROPERTY(ReplicatedUsing = OnRep_MaxResource)
	FGameplayAttributeData MaxResource;

	/** Meta attribute: a cost to take from Resource. Not replicated. */
	UPROPERTY()
	FGameplayAttributeData ResourceSpend;
};
