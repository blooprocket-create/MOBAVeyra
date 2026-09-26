// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attributes/VeyraVitalsSet.h"

#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "Tags/VeyraStatusTags.h"
#include "VeyraCombatLog.h"

FGameplayAttribute UVeyraVitalsSet::GetIncomingDamageAttribute(EVeyraDamageType Type)
{
	switch (Type)
	{
	case EVeyraDamageType::Physical:
		return GetIncomingPhysicalDamageAttribute();
	case EVeyraDamageType::Magic:
		return GetIncomingMagicDamageAttribute();
	case EVeyraDamageType::True:
		return GetIncomingTrueDamageAttribute();
	}
	return FGameplayAttribute();
}

TOptional<EVeyraDamageType> UVeyraVitalsSet::GetIncomingDamageType(const FGameplayAttribute& Attribute)
{
	for (const EVeyraDamageType Type : { EVeyraDamageType::Physical, EVeyraDamageType::Magic, EVeyraDamageType::True })
	{
		if (Attribute == GetIncomingDamageAttribute(Type))
		{
			return Type;
		}
	}
	return {};
}

void UVeyraVitalsSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.RepNotifyCondition = REPNOTIFY_Always;
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraVitalsSet, Health, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraVitalsSet, MaxHealth, Params);
}

void UVeyraVitalsSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetMaxHealthAttribute() && NewValue <= 0.0f)
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("Base Max Health on %s would become %g; it must stay above 0 (Combat Bible §39), so it keeps %g."),
			*GetNameSafe(GetOwningActor()), NewValue, MaxHealth.GetBaseValue());
		NewValue = MaxHealth.GetBaseValue();
	}
}

void UVeyraVitalsSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetMaxHealthAttribute() && NewValue <= 0.0f)
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("Max Health on %s would become %g; it must stay above 0 (Combat Bible §39), so it keeps %g."),
			*GetNameSafe(GetOwningActor()), NewValue, GetMaxHealth());
		NewValue = GetMaxHealth();
	}
}

void UVeyraVitalsSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	// Changing Max Health keeps the Health percentage (Combat Bible §41). Before the first
	// initialization there is no percentage to keep.
	if (Attribute == GetMaxHealthAttribute() && OldValue > 0.0f && NewValue != OldValue)
	{
		const AActor* Owner = GetOwningActor();
		if (Owner && Owner->HasAuthority())
		{
			SetHealth(GetHealth() * NewValue / OldValue);
		}
	}
}

bool UVeyraVitalsSet::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)
{
	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("An effect tried to change Health on %s directly; Health changes only through the damage pipeline."),
			*GetNameSafe(GetOwningActor()));
		return false;
	}
	return true;
}

void UVeyraVitalsSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	const TOptional<EVeyraDamageType> Type = GetIncomingDamageType(Data.EvaluatedData.Attribute);
	if (!Type.IsSet())
	{
		return;
	}

	// A meta attribute carries one damage component; read it, then clear it.
	UAbilitySystemComponent* AbilitySystem = GetOwningAbilitySystemComponent();
	const FGameplayAttribute Attribute = GetIncomingDamageAttribute(Type.GetValue());
	const float Amount = Attribute.GetNumericValue(this);
	AbilitySystem->SetNumericAttributeBase(Attribute, 0.0f);

	AActor* Owner = GetOwningActor();
	UVeyraDamageAbsorptionComponent* Absorption = Owner ? Owner->FindComponentByClass<UVeyraDamageAbsorptionComponent>() : nullptr;
	if (!Absorption)
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("%s has no UVeyraDamageAbsorptionComponent, so %g damage was not applied."), *GetNameSafe(Owner), Amount);
		return;
	}

	const bool bInvulnerable = AbilitySystem->HasMatchingGameplayTag(VeyraTags::Status_Invulnerable);
	const FVeyraAbsorptionResult Result = Absorption->ApplyIncomingDamage(Type.GetValue(), Amount, bInvulnerable, GetHealth());
	if (Result.HealthLost > 0.0)
	{
		SetHealth(GetHealth() - static_cast<float>(Result.HealthLost));
	}
}

void UVeyraVitalsSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVeyraVitalsSet, Health, OldValue);
}

void UVeyraVitalsSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVeyraVitalsSet, MaxHealth, OldValue);
}
