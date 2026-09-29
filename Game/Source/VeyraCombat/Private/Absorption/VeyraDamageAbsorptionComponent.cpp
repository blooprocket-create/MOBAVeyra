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
#include "Targeting/VeyraParticipantData.h"

namespace
{
	// Shield effects take their amount from SetByCaller data, never from level curves.
	constexpr float ShieldEffectLevel = 1.0f;

	bool IsValidShieldGrant(const FVeyraShieldGrant& Grant)
	{
		const bool bAmounts = FMath::IsFinite(Grant.Amount) && Grant.Amount > 0.0 && FMath::IsFinite(Grant.MaxAmount) && Grant.MaxAmount >= Grant.Amount;
		const bool bDuration = FMath::IsFinite(Grant.DurationSeconds) && Grant.DurationSeconds > 0.0;
		const bool bGroup = Grant.CapGroup.IsValid() ? (FMath::IsFinite(Grant.CapGroupTotal) && Grant.CapGroupTotal > 0.0) : Grant.CapGroupTotal == 0.0;
		return bAmounts && bDuration && bGroup;
	}
}

UVeyraDamageAbsorptionComponent::UVeyraDamageAbsorptionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

ELifetimeCondition UVeyraDamageAbsorptionComponent::GetReplicationCondition() const
{
	return VeyraParticipantData::ConditionFor(*this, Super::GetReplicationCondition());
}

void UVeyraDamageAbsorptionComponent::ReadyForReplication()
{
	Super::ReadyForReplication();
	if (VeyraParticipantData::IsParticipantData(*this) && GetOwner()->HasAuthority())
	{
		VeyraParticipantData::Gate(*this, *GetOwner());
	}
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

FActiveGameplayEffectHandle UVeyraDamageAbsorptionComponent::GrantShield(UAbilitySystemComponent& Source, const FVeyraShieldGrant& Grant)
{
	const AActor* Owner = GetOwner();
	UAbilitySystemComponent* Target = BoundAbilitySystem.Get();
	if (!Owner || !Owner->HasAuthority() || !Target || !IsValidShieldGrant(Grant))
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("Refused shield %s of %g for %g s on %s: it needs the server, a bound unit, an amount and duration above 0, a maximum of at least the amount, and a cap group total above 0 exactly when it has a group."),
			*Grant.Id.ToString(), Grant.Amount, Grant.DurationSeconds, *GetNameSafe(Owner));
		return FActiveGameplayEffectHandle();
	}

	// The shield this grant meets: the same identity from the same source.
	const FVeyraShieldEntry* Existing = nullptr;
	if (Grant.Id.IsValid())
	{
		Existing = Ledger.Shields.FindByPredicate([this, &Grant, &Source](const FVeyraShieldEntry& Entry) {
			const FServerEntry& Server = ServerEntries.FindChecked(Entry.Sequence);
			return Server.Id == Grant.Id && Server.Source.Get() == &Source;
		});
	}
	const bool bMerges = Existing && Grant.Reapply == EVeyraShieldReapply::Merge;
	double Amount = FMath::Min(bMerges ? Existing->Remaining + Grant.Amount : Grant.Amount, Grant.MaxAmount);
	if (Grant.CapGroup.IsValid())
	{
		double OthersInGroup = 0.0;
		for (const FVeyraShieldEntry& Entry : Ledger.Shields)
		{
			const FServerEntry& Server = ServerEntries.FindChecked(Entry.Sequence);
			if (&Entry != Existing && Server.CapGroup == Grant.CapGroup && Server.Source.Get() == &Source)
			{
				OthersInGroup += Entry.Remaining;
			}
		}
		Amount = FMath::Min(Amount, Grant.CapGroupTotal - OthersInGroup);
	}
	if (bMerges)
	{
		Amount = FMath::Max(Amount, Existing->Remaining);
	}
	if (Amount <= 0.0)
	{
		UE_LOG(LogVeyraCombat, Verbose, TEXT("Shield %s on %s granted nothing: its cap group is full."), *Grant.Id.ToString(), *Owner->GetName());
		return FActiveGameplayEffectHandle();
	}

	const FGameplayEffectSpecHandle Spec = Source.MakeOutgoingSpec(UVeyraShieldEffect::StaticClass(), ShieldEffectLevel, Source.MakeEffectContext());
	if (!Spec.IsValid())
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("Could not create shield %s for %s."), *Grant.Id.ToString(), *Owner->GetName());
		return FActiveGameplayEffectHandle();
	}
	Spec.Data->SetSetByCallerMagnitude(VeyraCombatTagMapping::ShieldCategoryTag(Grant.Category), static_cast<float>(Amount));
	Spec.Data->SetDuration(static_cast<float>(Grant.DurationSeconds), /*bLockDuration*/ true);

	// OnEffectAdded records the new effect with this grant's identity, in place of the old shield's
	// effect when there is one.
	const int32 ReplacesSequence = Existing ? Existing->Sequence : INDEX_NONE;
	const FActiveGameplayEffectHandle Replaced = Existing ? ServerEntries.FindChecked(ReplacesSequence).Effect : FActiveGameplayEffectHandle();
	PendingGrant = FPendingGrant{ Grant.Id, &Source, Grant.CapGroup, ReplacesSequence };
	const FActiveGameplayEffectHandle Effect = Source.ApplyGameplayEffectSpecToTarget(*Spec.Data, Target);
	PendingGrant.Reset();
	if (Effect.IsValid() && Replaced.IsValid())
	{
		Target->RemoveActiveGameplayEffect(Replaced);
	}
	return Effect;
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
			FServerEntry Server;
			if (ServerEntries.RemoveAndCopyValue(Sequence, Server) && AbilitySystem)
			{
				AbilitySystem->RemoveActiveGameplayEffect(Server.Effect);
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

	FServerEntry Server;
	Server.Effect = Handle;
	int32 Sequence = NextSequence;
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

		FVeyraShieldEntry* Entry = nullptr;
		if (PendingGrant.IsSet())
		{
			Server.Id = PendingGrant->Id;
			Server.Source = PendingGrant->Source;
			Server.CapGroup = PendingGrant->CapGroup;
			if (PendingGrant->ReplacesSequence != INDEX_NONE)
			{
				Sequence = PendingGrant->ReplacesSequence;
				Entry = Ledger.Shields.FindByPredicate([Sequence](const FVeyraShieldEntry& Candidate) { return Candidate.Sequence == Sequence; });
			}
		}
		if (!Entry)
		{
			Entry = &Ledger.Shields.AddDefaulted_GetRef();
			Entry->Sequence = NextSequence++;
		}
		Entry->Category = Category.GetValue();
		Entry->Remaining = Amount;
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
		Grant.Sequence = NextSequence++;
		Grant.Remaining = *Amount;
	}
	else
	{
		return;
	}

	// Replacing the entry's effect here means removing the old effect afterwards finds no entry.
	ServerEntries.Add(Sequence, Server);
	MarkLedgerDirty();
}

void UVeyraDamageAbsorptionComponent::OnEffectRemoved(const FActiveGameplayEffect& Effect)
{
	int32 Removed = INDEX_NONE;
	for (const TPair<int32, FServerEntry>& Pair : ServerEntries)
	{
		if (Pair.Value.Effect == Effect.Handle)
		{
			Removed = Pair.Key;
			break;
		}
	}
	if (Removed == INDEX_NONE)
	{
		return;
	}
	ServerEntries.Remove(Removed);
	Ledger.Shields.RemoveAll([Removed](const FVeyraShieldEntry& Entry) { return Entry.Sequence == Removed; });
	Ledger.TemporaryHealth.RemoveAll([Removed](const FVeyraTemporaryHealthGrant& Grant) { return Grant.Sequence == Removed; });
	MarkLedgerDirty();
}

void UVeyraDamageAbsorptionComponent::MarkLedgerDirty()
{
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraDamageAbsorptionComponent, Ledger, this);
}
