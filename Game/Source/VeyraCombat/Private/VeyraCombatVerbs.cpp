// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraCombatVerbs.h"

#include "AbilitySystemComponent.h"
#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "Attributes/VeyraAttributePolicy.h"
#include "Attributes/VeyraMobilitySet.h"
#include "Attributes/VeyraResourceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Effects/VeyraCombatEffects.h"
#include "Effects/VeyraResourceSpendExecution.h"
#include "Life/VeyraLifeComponent.h"
#include "Tags/VeyraHealthTags.h"
#include "VeyraCombatLog.h"
#include "VeyraCombatTagMapping.h"

namespace VeyraCombat
{
namespace
{
	// Veyra effects take every magnitude from SetByCaller data, never from Gameplay Ability System
	// level curves, so each spec uses the system's default effect level.
	constexpr float UnscaledEffectLevel = 1.0f;

	bool IsPositiveFinite(double Value)
	{
		return FMath::IsFinite(Value) && Value > 0.0;
	}

	FActiveGameplayEffectHandle GrantAbsorption(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target,
		TSubclassOf<UGameplayEffect> EffectClass, const FGameplayTag& AmountTag, double Amount, double DurationSeconds, const TCHAR* What)
	{
		if (!IsPositiveFinite(Amount) || !IsPositiveFinite(DurationSeconds))
		{
			UE_LOG(LogVeyraCombat, Error, TEXT("Refused %s of %g for %g s on %s: both must be finite and above 0."),
				What, Amount, DurationSeconds, *GetNameSafe(Target.GetOwner()));
			return FActiveGameplayEffectHandle();
		}
		const FGameplayEffectSpecHandle Spec = Source.MakeOutgoingSpec(EffectClass, UnscaledEffectLevel, Source.MakeEffectContext());
		if (!Spec.IsValid())
		{
			UE_LOG(LogVeyraCombat, Error, TEXT("Could not create %s for %s."), What, *GetNameSafe(Target.GetOwner()));
			return FActiveGameplayEffectHandle();
		}
		Spec.Data->SetSetByCallerMagnitude(AmountTag, static_cast<float>(Amount));
		Spec.Data->SetDuration(static_cast<float>(DurationSeconds), /*bLockDuration*/ true);
		return Source.ApplyGameplayEffectSpecToTarget(*Spec.Data, &Target);
	}
}

void ConfigureCombatant(UAbilitySystemComponent& AbilitySystem, UVeyraDamageAbsorptionComponent& Absorption)
{
	AbilitySystem.GameplayEffectApplicationQueries.Add(FGameplayEffectApplicationQuery::CreateStatic(&VeyraAttributePolicy::AllowsSpec));
	Absorption.BindTo(AbilitySystem);
}

bool InitializeVitals(UAbilitySystemComponent& AbilitySystem, double MaxHealth)
{
	if (!AbilitySystem.GetSet<UVeyraVitalsSet>() || !IsPositiveFinite(MaxHealth))
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("Refused to initialize vitals on %s with Max Health %g: it needs a UVeyraVitalsSet and a finite Max Health above 0."),
			*GetNameSafe(AbilitySystem.GetOwner()), MaxHealth);
		return false;
	}
	AbilitySystem.SetNumericAttributeBase(UVeyraVitalsSet::GetMaxHealthAttribute(), static_cast<float>(MaxHealth));
	AbilitySystem.SetNumericAttributeBase(UVeyraVitalsSet::GetHealthAttribute(), AbilitySystem.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()));
	return true;
}

bool InitializeMoveSpeed(UAbilitySystemComponent& AbilitySystem, double MoveSpeed)
{
	if (!AbilitySystem.GetSet<UVeyraMobilitySet>() || !IsPositiveFinite(MoveSpeed))
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("Refused to initialize Move Speed on %s with %g: it needs a UVeyraMobilitySet and a finite speed above 0."),
			*GetNameSafe(AbilitySystem.GetOwner()), MoveSpeed);
		return false;
	}
	AbilitySystem.SetNumericAttributeBase(UVeyraMobilitySet::GetMoveSpeedAttribute(), static_cast<float>(MoveSpeed));
	return true;
}

bool InitializeResource(UAbilitySystemComponent& AbilitySystem, double MaxResource)
{
	if (!AbilitySystem.GetSet<UVeyraResourceSet>() || !FMath::IsFinite(MaxResource) || MaxResource < 0.0)
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("Refused to initialize the resource on %s with Max Resource %g: it needs a UVeyraResourceSet and a finite Max Resource of at least 0."),
			*GetNameSafe(AbilitySystem.GetOwner()), MaxResource);
		return false;
	}
	AbilitySystem.SetNumericAttributeBase(UVeyraResourceSet::GetMaxResourceAttribute(), static_cast<float>(MaxResource));
	AbilitySystem.SetNumericAttributeBase(UVeyraResourceSet::GetResourceAttribute(), AbilitySystem.GetNumericAttribute(UVeyraResourceSet::GetMaxResourceAttribute()));
	return true;
}

bool CanAffordResource(const UAbilitySystemComponent& AbilitySystem, double Amount)
{
	return Amount <= 0.0 || (AbilitySystem.GetSet<UVeyraResourceSet>() && AbilitySystem.GetNumericAttribute(UVeyraResourceSet::GetResourceAttribute()) >= Amount);
}

bool SpendResource(UAbilitySystemComponent& AbilitySystem, double Amount)
{
	if (!FMath::IsFinite(Amount) || Amount < 0.0)
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("Refused a resource cost of %g on %s: it must be finite and at least 0."), Amount, *GetNameSafe(AbilitySystem.GetOwner()));
		return false;
	}
	if (!CanAffordResource(AbilitySystem, Amount))
	{
		return false;
	}
	if (Amount == 0.0)
	{
		return true;
	}
	const FGameplayEffectSpecHandle Spec = AbilitySystem.MakeOutgoingSpec(UVeyraResourceSpendEffect::StaticClass(), UnscaledEffectLevel, AbilitySystem.MakeEffectContext());
	if (!Spec.IsValid())
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("Could not create a resource cost for %s."), *GetNameSafe(AbilitySystem.GetOwner()));
		return false;
	}
	Spec.Data->SetSetByCallerMagnitude(UVeyraResourceSpendExecution::ResourceCostName, static_cast<float>(Amount));
	AbilitySystem.ApplyGameplayEffectSpecToSelf(*Spec.Data);
	return true;
}

bool Revive(UAbilitySystemComponent& AbilitySystem)
{
	AActor* Owner = AbilitySystem.GetOwner();
	UVeyraLifeComponent* Life = Owner ? Owner->FindComponentByClass<UVeyraLifeComponent>() : nullptr;
	if (!Life || !Life->SetState(EVeyraLifeState::Alive))
	{
		return false;
	}
	AbilitySystem.SetNumericAttributeBase(UVeyraVitalsSet::GetHealthAttribute(), AbilitySystem.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()));
	if (AbilitySystem.GetSet<UVeyraResourceSet>())
	{
		AbilitySystem.SetNumericAttributeBase(UVeyraResourceSet::GetResourceAttribute(), AbilitySystem.GetNumericAttribute(UVeyraResourceSet::GetMaxResourceAttribute()));
	}
	return true;
}

bool DealDamage(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target, const FVeyraRawDamageEvent& Damage)
{
	const AActor* TargetOwner = Target.GetOwner();
	const UVeyraLifeComponent* TargetLife = TargetOwner ? TargetOwner->FindComponentByClass<UVeyraLifeComponent>() : nullptr;
	if (TargetLife && !TargetLife->IsAlive())
	{
		UE_LOG(LogVeyraCombat, Verbose, TEXT("Ignored damage to %s: its death is final."), *GetNameSafe(TargetOwner));
		return false;
	}

	TArray<EVeyraDamageType, TInlineAllocator<3>> Types;
	bool bValid = !Damage.Components.IsEmpty();
	for (const FVeyraDamageComponent& Component : Damage.Components)
	{
		bValid &= !Types.Contains(Component.Type) && FMath::IsFinite(Component.Amount) && Component.Amount >= 0.0;
		Types.Add(Component.Type);
	}
	if (!bValid)
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("Refused damage to %s: it needs at least one component, each type at most once, with a finite amount of at least 0."),
			*GetNameSafe(Target.GetOwner()));
		return false;
	}

	const FGameplayEffectSpecHandle Spec = Source.MakeOutgoingSpec(UVeyraDamageEffect::StaticClass(), UnscaledEffectLevel, Source.MakeEffectContext());
	if (!Spec.IsValid())
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("Could not create a damage effect for %s."), *GetNameSafe(Target.GetOwner()));
		return false;
	}
	for (const FVeyraDamageComponent& Component : Damage.Components)
	{
		Spec.Data->SetSetByCallerMagnitude(VeyraCombatTagMapping::DamageTypeTag(Component.Type), static_cast<float>(Component.Amount));
	}
	return Source.ApplyGameplayEffectSpecToTarget(*Spec.Data, &Target).WasSuccessfullyApplied();
}

FActiveGameplayEffectHandle GrantShield(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target, EVeyraShieldCategory Category,
	double Amount, double DurationSeconds)
{
	return GrantAbsorption(Source, Target, UVeyraShieldEffect::StaticClass(), VeyraCombatTagMapping::ShieldCategoryTag(Category), Amount,
		DurationSeconds, TEXT("a shield"));
}

FActiveGameplayEffectHandle GrantTemporaryHealth(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target, double Amount, double DurationSeconds)
{
	return GrantAbsorption(Source, Target, UVeyraTemporaryHealthEffect::StaticClass(), VeyraTags::TemporaryHealth, Amount, DurationSeconds,
		TEXT("Temporary Health"));
}
}
