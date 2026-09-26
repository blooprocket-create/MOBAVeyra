// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attributes/VeyraResourceSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "VeyraCombatLog.h"

void UVeyraResourceSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.RepNotifyCondition = REPNOTIFY_Always;
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraResourceSet, Resource, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraResourceSet, MaxResource, Params);
}

void UVeyraResourceSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetResourceAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxResource());
	}
	else if (Attribute == GetMaxResourceAttribute() && NewValue < 0.0f)
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("Base Max Resource on %s would become %g; it cannot be negative, so it keeps %g."),
			*GetNameSafe(GetOwningActor()), NewValue, MaxResource.GetBaseValue());
		NewValue = MaxResource.GetBaseValue();
	}
}

void UVeyraResourceSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	if (Attribute == GetResourceAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxResource());
	}
	else if (Attribute == GetMaxResourceAttribute() && NewValue < 0.0f)
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("Max Resource on %s would become %g; it cannot be negative, so it keeps %g."),
			*GetNameSafe(GetOwningActor()), NewValue, GetMaxResource());
		NewValue = GetMaxResource();
	}
}

void UVeyraResourceSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	// A lower Max Resource never leaves Resource above it. (Canon says nothing about keeping the
	// percentage, unlike Health's §41 rule, so a higher maximum leaves Resource as it is.)
	if (Attribute == GetMaxResourceAttribute() && GetResource() > NewValue)
	{
		const AActor* Owner = GetOwningActor();
		if (Owner && Owner->HasAuthority())
		{
			SetResource(NewValue);
		}
	}
}

bool UVeyraResourceSet::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)
{
	if (Data.EvaluatedData.Attribute == GetResourceAttribute())
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("An effect tried to change Resource on %s directly; costs go through VeyraCombat::SpendResource."),
			*GetNameSafe(GetOwningActor()));
		return false;
	}
	return true;
}

void UVeyraResourceSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	if (Data.EvaluatedData.Attribute != GetResourceSpendAttribute())
	{
		return;
	}
	const float Spend = GetResourceSpend();
	GetOwningAbilitySystemComponent()->SetNumericAttributeBase(GetResourceSpendAttribute(), 0.0f);
	SetResource(GetResource() - Spend);
}

void UVeyraResourceSet::OnRep_Resource(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVeyraResourceSet, Resource, OldValue);
}

void UVeyraResourceSet::OnRep_MaxResource(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVeyraResourceSet, MaxResource, OldValue);
}
