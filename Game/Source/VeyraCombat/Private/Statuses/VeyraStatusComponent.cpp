// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Statuses/VeyraStatusComponent.h"

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraDefenceSet.h"
#include "Effects/VeyraCombatEffects.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Records/VeyraCombatRecords.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"
#include "VeyraCombatLog.h"

namespace
{
	// Status effects take every magnitude from SetByCaller data, never from level curves.
	constexpr float StatusEffectLevel = 1.0f;

	/** The SetByCaller name of the modifier a stat kind drives, or None for a kind that changes no stat. */
	FName StatusMultiplierName(EVeyraStatusKind Kind)
	{
		switch (Kind)
		{
		case EVeyraStatusKind::MoveSpeed:
			return UVeyraStatusEffect::MoveSpeedMultiplierName;
		case EVeyraStatusKind::AttackSpeed:
			return UVeyraStatusEffect::AttackSpeedMultiplierName;
		case EVeyraStatusKind::Tenacity:
			return UVeyraStatusEffect::TenacityMultiplierName;
		case EVeyraStatusKind::DamageReduction:
			return UVeyraStatusEffect::IncomingDamageMultiplierName;
		case EVeyraStatusKind::DisplacementResistance:
			return UVeyraStatusEffect::DisplacementMultiplierName;
		case EVeyraStatusKind::Stun:
		case EVeyraStatusKind::Slow:
		case EVeyraStatusKind::AttackCleave:
		case EVeyraStatusKind::MoveSpeedTowardEnemyVanguards:
			break;
		}
		return NAME_None;
	}
}

UVeyraStatusComponent::UVeyraStatusComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UVeyraStatusComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Every machine sees every unit's statuses until Vision gates them (ADR-009 §7).
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraStatusComponent, Ledger, Params);
}

void UVeyraStatusComponent::BindTo(UAbilitySystemComponent& AbilitySystem)
{
	BoundAbilitySystem = &AbilitySystem;
	AbilitySystem.OnAnyGameplayEffectRemovedDelegate().AddUObject(this, &UVeyraStatusComponent::OnEffectRemoved);
}

bool UVeyraStatusComponent::Apply(UAbilitySystemComponent& Source, const FVeyraStatusSpec& Spec)
{
	const AActor* Owner = GetOwner();
	UAbilitySystemComponent* Target = BoundAbilitySystem.Get();
	const TArray<FString> Problems = VeyraStatuses::Validate(Spec);
	if (!Owner || !Owner->HasAuthority() || !Target || !Problems.IsEmpty())
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("Refused status %s on %s: %s."), *Spec.Id.ToString(), *GetNameSafe(Owner),
			Problems.IsEmpty() ? TEXT("statuses are applied on the server, to a unit whose Ability System Component is bound") : *FString::Join(Problems, TEXT("; ")));
		return false;
	}

	const int32 ActiveIndex = FindActive(Spec, Source);
	if (ActiveIndex != INDEX_NONE && Ledger.Entries[ActiveIndex].Kind != Spec.Kind)
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("Refused status %s on %s: it is already active as a different kind; one ID is one status (Combat Bible §46)."),
			*Spec.Id.ToString(), *Owner->GetName());
		return false;
	}

	double DurationSeconds = Spec.DurationSeconds;
	if (VeyraStatuses::IsTenacityReducible(Spec.Kind))
	{
		DurationSeconds = VeyraStatuses::ApplyTenacity(DurationSeconds, Target->GetNumericAttribute(UVeyraDefenceSet::GetTenacityRetainedAttribute()),
			UVeyraCombatTuningSubsystem::Get().CrowdControl.TenacityFloorSeconds);
	}
	const double Now = GetServerNow();
	const double EndsAt = Now + DurationSeconds;

	int32 Stacks = 1;
	if (ActiveIndex != INDEX_NONE)
	{
		const FVeyraStatusEntry& Active = Ledger.Entries[ActiveIndex];
		if (Spec.Stacking == EVeyraStackingPolicy::UniqueReplaceStrongest && !VeyraStatuses::IsStronger(Active, Spec.Magnitude, EndsAt))
		{
			return true;
		}
		if (Spec.Stacking == EVeyraStackingPolicy::Stacking)
		{
			Stacks = FMath::Min(Active.Stacks + 1, Spec.MaxStacks);
		}
	}

	// The new effect goes on before the old one comes off, so a refused effect leaves the status as it was.
	const FActiveGameplayEffectHandle Effect = ApplyEffect(Source, *Target, Spec.Kind, Spec.Magnitude, Stacks, DurationSeconds);
	if (!Effect.IsValid())
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("Refused status %s on %s: its Gameplay Effect was not applied."), *Spec.Id.ToString(), *Owner->GetName());
		return false;
	}

	FVeyraStatusEntry* Entry = nullptr;
	FActiveGameplayEffectHandle Replaced;
	if (ActiveIndex != INDEX_NONE)
	{
		Entry = &Ledger.Entries[ActiveIndex];
		Replaced = ServerEntries.FindChecked(Entry->Sequence).Effect;
	}
	else
	{
		Entry = &Ledger.Entries.AddDefaulted_GetRef();
		Entry->Sequence = NextSequence++;
		Entry->Id = Spec.Id;
		Entry->Kind = Spec.Kind;
	}
	// A new application starts its takedown extensions afresh.
	ServerEntries.Add(Entry->Sequence, FServerEntry{ Effect, &Source, Spec.TakedownExtensionSeconds, Spec.TakedownExtensionMaxSeconds });
	Entry->Magnitude = Spec.Magnitude;
	Entry->Stacks = Stacks;
	Entry->StartedAt = Now;
	Entry->EndsAt = EndsAt;
	// The entry points at its new effect already, so removing the old one leaves the entry in place.
	if (Replaced.IsValid())
	{
		Target->RemoveActiveGameplayEffect(Replaced);
	}
	MarkLedgerChanged();

	VeyraCombatRecords::NoteHostileAction(&Source, *Target);
	if (Spec.Kind == EVeyraStatusKind::Stun)
	{
		NotifyInterrupted();
	}
	return true;
}

void UVeyraStatusComponent::NotifyInterrupted()
{
	OnInterrupted.Broadcast();
}

void UVeyraStatusComponent::ExtendForTakedown()
{
	UAbilitySystemComponent* Target = BoundAbilitySystem.Get();
	bool bExtended = false;
	for (FVeyraStatusEntry& Entry : Ledger.Entries)
	{
		FServerEntry& Server = ServerEntries.FindChecked(Entry.Sequence);
		const double Extension = FMath::Min(Server.TakedownExtensionSeconds, Server.TakedownExtensionMaxSeconds - Server.ExtendedSeconds);
		if (!Target || Extension <= 0.0)
		{
			continue;
		}
		// A later start leaves the effect longer to run; the world clock keeps counting it.
		Target->ModifyActiveEffectStartTime(Server.Effect, static_cast<float>(Extension));
		Server.ExtendedSeconds += Extension;
		Entry.EndsAt += Extension;
		bExtended = true;
	}
	if (bExtended)
	{
		MarkLedgerChanged();
	}
}

bool UVeyraStatusComponent::Remove(const FVeyraContentId& Id)
{
	UAbilitySystemComponent* Target = BoundAbilitySystem.Get();
	TArray<FActiveGameplayEffectHandle, TInlineAllocator<2>> Ended;
	for (int32 Index = Ledger.Entries.Num() - 1; Index >= 0; --Index)
	{
		if (Ledger.Entries[Index].Id == Id)
		{
			FServerEntry Server;
			if (ServerEntries.RemoveAndCopyValue(Ledger.Entries[Index].Sequence, Server))
			{
				Ended.Add(Server.Effect);
			}
			Ledger.Entries.RemoveAt(Index);
		}
	}
	if (Ended.IsEmpty())
	{
		return false;
	}
	// The entries are gone already, so their effects' removal finds nothing more to do.
	for (const FActiveGameplayEffectHandle& Effect : Ended)
	{
		if (Target)
		{
			Target->RemoveActiveGameplayEffect(Effect);
		}
	}
	MarkLedgerChanged();
	return true;
}

double UVeyraStatusComponent::GetStrongestSlow() const
{
	return VeyraStatuses::StrongestSlow(Ledger.Entries);
}

double UVeyraStatusComponent::GetStrongest(EVeyraStatusKind Kind) const
{
	return VeyraStatuses::Strongest(Ledger.Entries, Kind);
}

EVeyraActionBlocks UVeyraStatusComponent::GetActionBlocks() const
{
	return VeyraStatuses::ActionBlocks(Ledger.Entries);
}

void UVeyraStatusComponent::OnRep_Ledger()
{
	OnStatusesChanged.Broadcast();
}

void UVeyraStatusComponent::OnEffectRemoved(const FActiveGameplayEffect& Effect)
{
	int32 Ended = INDEX_NONE;
	for (const TPair<int32, FServerEntry>& Pair : ServerEntries)
	{
		if (Pair.Value.Effect == Effect.Handle)
		{
			Ended = Pair.Key;
			break;
		}
	}
	if (Ended == INDEX_NONE)
	{
		return;
	}
	ServerEntries.Remove(Ended);
	Ledger.Entries.RemoveAll([Ended](const FVeyraStatusEntry& Entry) { return Entry.Sequence == Ended; });
	MarkLedgerChanged();
}

FActiveGameplayEffectHandle UVeyraStatusComponent::ApplyEffect(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target,
	EVeyraStatusKind Kind, double Magnitude, int32 Stacks, double DurationSeconds) const
{
	const FName Line = StatusMultiplierName(Kind);
	const TSubclassOf<UGameplayEffect> EffectClass = Line.IsNone() ? UVeyraStatusMarkerEffect::StaticClass() : UVeyraStatusEffect::StaticClass();
	const FGameplayEffectSpecHandle Spec = Source.MakeOutgoingSpec(EffectClass, StatusEffectLevel, Source.MakeEffectContext());
	if (!Spec.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}
	if (!Line.IsNone())
	{
		// Every line is set, as the effect requires; only the status's own stat changes.
		constexpr float Unchanged = 1.0f;
		for (const FName Name : { UVeyraStatusEffect::MoveSpeedMultiplierName, UVeyraStatusEffect::AttackSpeedMultiplierName,
				 UVeyraStatusEffect::TenacityMultiplierName, UVeyraStatusEffect::IncomingDamageMultiplierName, UVeyraStatusEffect::DisplacementMultiplierName })
		{
			Spec.Data->SetSetByCallerMagnitude(Name, Unchanged);
		}
		Spec.Data->SetSetByCallerMagnitude(Line, static_cast<float>(VeyraStatuses::StatMultiplier(Kind, Magnitude, Stacks)));
	}
	Spec.Data->SetDuration(static_cast<float>(DurationSeconds), /*bLockDuration*/ true);
	return Source.ApplyGameplayEffectSpecToTarget(*Spec.Data, &Target);
}

int32 UVeyraStatusComponent::FindActive(const FVeyraStatusSpec& Spec, const UAbilitySystemComponent& Source) const
{
	return Ledger.Entries.IndexOfByPredicate([this, &Spec, &Source](const FVeyraStatusEntry& Entry) {
		if (!(Entry.Id == Spec.Id))
		{
			return false;
		}
		// Independent sources keep one instance each; every other policy keeps one per unit.
		const FServerEntry& Server = ServerEntries.FindChecked(Entry.Sequence);
		return Spec.Stacking != EVeyraStackingPolicy::IndependentSources || Server.Source.Get() == &Source;
	});
}

void UVeyraStatusComponent::MarkLedgerChanged()
{
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraStatusComponent, Ledger, this);
	OnStatusesChanged.Broadcast();
}

double UVeyraStatusComponent::GetServerNow() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
}
