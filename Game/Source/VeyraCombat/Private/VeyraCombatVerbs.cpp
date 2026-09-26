// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraCombatVerbs.h"

#include "AbilitySystemComponent.h"
#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "Attributes/VeyraAttributePolicy.h"
#include "Attributes/VeyraMobilitySet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Effects/VeyraCombatEffects.h"
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

bool DealDamage(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target, const FVeyraRawDamageEvent& Damage)
{
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
