// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Absorption/VeyraDamageAbsorptionComponent.h"

#include "AbilitySystemComponent.h"
#include "Effects/VeyraCombatEffects.h"
#include "GameFramework/Actor.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Tags/VeyraHealthTags.h"
#include "VeyraCombatLog.h"
#include "VeyraCombatTagMapping.h"

UVeyraDamageAbsorptionComponent::UVeyraDamageAbsorptionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UVeyraDamageAbsorptionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraDamageAbsorptionComponent, Ledger, Params);
}

void UVeyraDamageAbsorptionComponent::BindTo(UAbilitySystemComponent& AbilitySystem)
{
	BoundAbilitySystem = &AbilitySystem;
	AbilitySystem.OnActiveGameplayEffectAddedDelegateToSelf.AddUObject(this, &UVeyraDamageAbsorptionComponent::OnEffectAdded);
	AbilitySystem.OnAnyGameplayEffectRemovedDelegate().AddUObject(this, &UVeyraDamageAbsorptionComponent::OnEffectRemoved);
}

FVeyraAbsorptionResult UVeyraDamageAbsorptionComponent::ApplyIncomingDamage(EVeyraDamageType Type, double Amount, bool bInvulnerable, double Health)
{
	const FVeyraAbsorptionResult Result = VeyraAbsorption::Absorb(Type, Amount, bInvulnerable, Ledger, Health);
	if (Result.ShieldAbsorbed > 0.0 || Result.TemporaryHealthSpent > 0.0)
	{
		MarkLedgerDirty();
	}

	// An emptied entry's effect ends with it. Its ledger entry is already gone.
	UAbilitySystemComponent* AbilitySystem = BoundAbilitySystem.Get();
	for (const TArray<int32>* Depleted : { &Result.DepletedShields, &Result.DepletedTemporaryHealth })
	{
		for (const int32 Sequence : *Depleted)
		{
			FActiveGameplayEffectHandle Handle;
			if (EffectsBySequence.RemoveAndCopyValue(Sequence, Handle) && AbilitySystem)
			{
				AbilitySystem->RemoveActiveGameplayEffect(Handle);
			}
		}
	}
	return Result;
}

void UVeyraDamageAbsorptionComponent::OnEffectAdded(UAbilitySystemComponent* AbilitySystem, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle)
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !Spec.Def)
	{
		return;
	}

	if (Spec.Def->IsA<UVeyraShieldEffect>())
	{
		TOptional<EVeyraShieldCategory> Category;
		double Amount = 0.0;
		int32 CategoryCount = 0;
		for (const TPair<FGameplayTag, float>& Magnitude : Spec.SetByCallerTagMagnitudes)
		{
			if (const TOptional<EVeyraShieldCategory> Found = VeyraCombatTagMapping::ShieldCategoryFromTag(Magnitude.Key))
			{
				Category = Found;
				Amount = Magnitude.Value;
				++CategoryCount;
			}
		}
		if (CategoryCount != 1 || !(Amount > 0.0) || !FMath::IsFinite(Amount))
		{
			UE_LOG(LogVeyraCombat, Error, TEXT("A shield effect on %s needs exactly one Shield.Type amount above 0; it absorbs nothing."), *Owner->GetName());
			return;
		}
		FVeyraShieldEntry& Entry = Ledger.Shields.AddDefaulted_GetRef();
		Entry.Sequence = NextSequence;
		Entry.Category = Category.GetValue();
		Entry.Remaining = Amount;
	}
	else if (Spec.Def->IsA<UVeyraTemporaryHealthEffect>())
	{
		const float* Amount = Spec.SetByCallerTagMagnitudes.Find(VeyraTags::TemporaryHealth);
		if (!Amount || !(*Amount > 0.0f) || !FMath::IsFinite(*Amount))
		{
			UE_LOG(LogVeyraCombat, Error, TEXT("A Temporary Health effect on %s needs a TemporaryHealth amount above 0; it grants nothing."), *Owner->GetName());
			return;
		}
		FVeyraTemporaryHealthGrant& Grant = Ledger.TemporaryHealth.AddDefaulted_GetRef();
		Grant.Sequence = NextSequence;
		Grant.Remaining = *Amount;
	}
	else
	{
		return;
	}

	EffectsBySequence.Add(NextSequence, Handle);
	++NextSequence;
	MarkLedgerDirty();
}

void UVeyraDamageAbsorptionComponent::OnEffectRemoved(const FActiveGameplayEffect& Effect)
{
	const int32* Sequence = EffectsBySequence.FindKey(Effect.Handle);
	if (!Sequence)
	{
		return;
	}
	const int32 Removed = *Sequence;
	EffectsBySequence.Remove(Removed);
	Ledger.Shields.RemoveAll([Removed](const FVeyraShieldEntry& Entry) { return Entry.Sequence == Removed; });
	Ledger.TemporaryHealth.RemoveAll([Removed](const FVeyraTemporaryHealthGrant& Grant) { return Grant.Sequence == Removed; });
	MarkLedgerDirty();
}

void UVeyraDamageAbsorptionComponent::MarkLedgerDirty()
{
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraDamageAbsorptionComponent, Ledger, this);
}
