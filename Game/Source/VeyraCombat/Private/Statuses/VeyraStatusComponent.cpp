// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Statuses/VeyraStatusComponent.h"

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraDefenceSet.h"
#include "Effects/VeyraCombatEffects.h"
#include "Engine/World.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "GameFramework/GameStateBase.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Records/VeyraCombatRecords.h"
#include "TimerManager.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"
#include "VeyraCombatLog.h"
#include "VeyraCombatVerbs.h"
#include "Targeting/VeyraParticipantData.h"

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
		case EVeyraStatusKind::HealthRegeneration:
			return UVeyraStatusEffect::HealthRegenMultiplierName;
		case EVeyraStatusKind::DamageAmplification:
		case EVeyraStatusKind::Weaken:
			return UVeyraStatusEffect::OutgoingDamageMultiplierName;
		case EVeyraStatusKind::MagicResistReduction:
			return UVeyraStatusEffect::MagicResistRetainedMultiplierName;
		case EVeyraStatusKind::Stun:
		case EVeyraStatusKind::Slow:
		case EVeyraStatusKind::AttackCleave:
		case EVeyraStatusKind::MoveSpeedTowardEnemyVanguards:
		case EVeyraStatusKind::DamageOverTime:
		case EVeyraStatusKind::AttackRange:
		case EVeyraStatusKind::AttackSpeedCap:
		case EVeyraStatusKind::SlowResistance:
		case EVeyraStatusKind::Planted:
		case EVeyraStatusKind::Camouflage:
		case EVeyraStatusKind::SourceAttackRange:
		case EVeyraStatusKind::Counter:
		case EVeyraStatusKind::Dormant:
		case EVeyraStatusKind::Unstoppable:
		case EVeyraStatusKind::DisplacementImmunity:
		case EVeyraStatusKind::Fear:
		case EVeyraStatusKind::Knockup:
		case EVeyraStatusKind::Ghosted:
		case EVeyraStatusKind::BodyScale:
		case EVeyraStatusKind::DirectionalDamageReduction:
		case EVeyraStatusKind::AttackDamageAmplification:
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

ELifetimeCondition UVeyraStatusComponent::GetReplicationCondition() const
{
	return VeyraParticipantData::ConditionFor(*this, Super::GetReplicationCondition());
}

void UVeyraStatusComponent::ReadyForReplication()
{
	Super::ReadyForReplication();
	if (VeyraParticipantData::IsParticipantData(*this) && GetOwner()->HasAuthority())
	{
		VeyraParticipantData::Gate(*this, *GetOwner());
	}
}

void UVeyraStatusComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// A participant's reach those who see it; a unit's, those its body reaches (ADR-016 §3).
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
	// A new application starts its takedown extensions, and its ticks, afresh (§14: a refresh restarts the duration).
	if (FServerEntry* Earlier = ServerEntries.Find(Entry->Sequence))
	{
		StopTicking(*Earlier, /*bDealDueTick*/ false);
	}
	FServerEntry& Server = ServerEntries.Add(Entry->Sequence, FServerEntry{ Effect, &Source, Spec.TakedownExtensionSeconds, Spec.TakedownExtensionMaxSeconds });
	Server.StackDecaySeconds = Spec.StackDecaySeconds;
	Server.ArcDegrees = Spec.ArcDegrees;
	Server.UnitKinds = Spec.UnitKinds;
	if (Spec.Kind == EVeyraStatusKind::DamageOverTime)
	{
		Server.TickDamageType = Spec.DamageType;
		Server.TickDamage = Spec.Magnitude * Stacks;
		Server.TickSeconds = Spec.TickSeconds;
		Server.TicksLeft = VeyraStatuses::TickCount(DurationSeconds, Spec.TickSeconds);
		StartTicking(Entry->Sequence, Server);
	}
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
	// What was applied, for statistics (ADR-017 §1).
	if (UVeyraCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		Events->OnStatusApplied.Broadcast(FVeyraStatusApplied{ &Source, Target, Spec.Kind, Now, EndsAt, Spec.Id });
	}
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
				// Removed early: what has not ticked yet never will (Combat Bible §14).
				StopTicking(Server, /*bDealDueTick*/ false);
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

double UVeyraStatusComponent::GetTotal(EVeyraStatusKind Kind) const
{
	return VeyraStatuses::Total(Ledger.Entries, Kind);
}

double UVeyraStatusComponent::GetRetained(EVeyraStatusKind Kind) const
{
	return VeyraStatuses::Retained(Ledger.Entries, Kind);
}

double UVeyraStatusComponent::GetTotalFrom(EVeyraStatusKind Kind, const UAbilitySystemComponent& Source) const
{
	double Sum = 0.0;
	for (const FVeyraStatusEntry& Entry : Ledger.Entries)
	{
		const FServerEntry* Server = Entry.Kind == Kind ? ServerEntries.Find(Entry.Sequence) : nullptr;
		if (Server && Server->Source.Get() == &Source)
		{
			Sum += Entry.Magnitude * Entry.Stacks;
		}
	}
	return Sum;
}

bool UVeyraStatusComponent::HasFrom(const FVeyraContentId& Id, const UAbilitySystemComponent& Source) const
{
	return Ledger.Entries.ContainsByPredicate([this, &Id, &Source](const FVeyraStatusEntry& Entry) {
		const FServerEntry* Server = Entry.Id == Id ? ServerEntries.Find(Entry.Sequence) : nullptr;
		return Server && Server->Source.Get() == &Source;
	});
}

int32 UVeyraStatusComponent::GetStacksFrom(const FVeyraContentId& Id, const UAbilitySystemComponent& Source) const
{
	int32 Stacks = 0;
	for (const FVeyraStatusEntry& Entry : Ledger.Entries)
	{
		const FServerEntry* Server = Entry.Id == Id ? ServerEntries.Find(Entry.Sequence) : nullptr;
		if (Server && Server->Source.Get() == &Source)
		{
			Stacks += Entry.Stacks;
		}
	}
	return Stacks;
}

bool UVeyraStatusComponent::Has(EVeyraStatusKind Kind) const
{
	return Ledger.Entries.ContainsByPredicate([Kind](const FVeyraStatusEntry& Entry) { return Entry.Kind == Kind; });
}

double UVeyraStatusComponent::GetDirectionalRetained(const FVector& Facing, const FVector& ToSource) const
{
	const FVector Ahead = Facing.GetSafeNormal2D();
	const FVector Toward = ToSource.GetSafeNormal2D();
	if (Ahead.IsNearlyZero() || Toward.IsNearlyZero())
	{
		return 1.0;
	}
	const double Off = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Ahead, Toward), -1.0, 1.0)));
	double Retained = 1.0;
	for (const FVeyraStatusEntry& Entry : Ledger.Entries)
	{
		const FServerEntry* Server = Entry.Kind == EVeyraStatusKind::DirectionalDamageReduction ? ServerEntries.Find(Entry.Sequence) : nullptr;
		if (Server && Off <= Server->ArcDegrees / 2.0)
		{
			Retained *= 1.0 - Entry.Magnitude * Entry.Stacks;
		}
	}
	return FMath::Max(Retained, 0.0);
}

double UVeyraStatusComponent::GetAttackAmplification(TOptional<EVeyraUnitKind> TargetKind) const
{
	double Added = 0.0;
	for (const FVeyraStatusEntry& Entry : Ledger.Entries)
	{
		const FServerEntry* Server = Entry.Kind == EVeyraStatusKind::AttackDamageAmplification ? ServerEntries.Find(Entry.Sequence) : nullptr;
		const bool bApplies = Server && (Server->UnitKinds.IsEmpty() || (TargetKind.IsSet() && Server->UnitKinds.Contains(TargetKind.GetValue())));
		if (bApplies)
		{
			Added += Entry.Magnitude * Entry.Stacks;
		}
	}
	return Added;
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
	// A status that decays a stack at a time loses one as its time runs out and runs again, until its
	// last stack goes (ADR-018 §2). Removed early, or its unit dead, it ends at once.
	if (DecayOneStack(Ended, Effect))
	{
		return;
	}
	FServerEntry Server;
	ServerEntries.RemoveAndCopyValue(Ended, Server);
	Ledger.Entries.RemoveAll([Ended](const FVeyraStatusEntry& Entry) { return Entry.Sequence == Ended; });
	MarkLedgerChanged();
	// Its time ran out, or its unit died; a dead unit takes no more damage, so only the former ticks.
	StopTicking(Server, /*bDealDueTick*/ true);
}

void UVeyraStatusComponent::StartTicking(int32 Sequence, FServerEntry& Server)
{
	UWorld* World = GetWorld();
	if (!World || Server.TicksLeft <= 0)
	{
		return;
	}
	// World time, so a pause holds the ticks as it holds the status's effect (ADR-006 §8).
	World->GetTimerManager().SetTimer(Server.TickTimer, FTimerDelegate::CreateUObject(this, &UVeyraStatusComponent::DealTick, Sequence),
		static_cast<float>(Server.TickSeconds), /*bLoop*/ true);
}

void UVeyraStatusComponent::DealTick(int32 Sequence)
{
	FServerEntry* Server = ServerEntries.Find(Sequence);
	if (!Server || Server->TicksLeft <= 0)
	{
		return;
	}
	--Server->TicksLeft;
	if (Server->TicksLeft == 0)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(Server->TickTimer);
		}
	}
	// Copied first: a lethal tick ends the unit's statuses, this one among them.
	const TWeakObjectPtr<UAbilitySystemComponent> Source = Server->Source;
	DealTickDamage(Source, Server->TickDamageType, Server->TickDamage);
}

void UVeyraStatusComponent::StopTicking(FServerEntry& Server, bool bDealDueTick)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(Server.TickTimer);
	}
	// Whole ticks fit its duration, so only the last can remain as its effect ends, due at that same
	// moment; the two timers run in either order (§14: no tick is lost).
	const bool bLastTickDue = bDealDueTick && Server.TicksLeft == 1;
	Server.TicksLeft = 0;
	if (bLastTickDue)
	{
		DealTickDamage(Server.Source, Server.TickDamageType, Server.TickDamage);
	}
}

void UVeyraStatusComponent::DealTickDamage(const TWeakObjectPtr<UAbilitySystemComponent>& Source, EVeyraDamageType Type, double Amount) const
{
	UAbilitySystemComponent* Attacker = Source.Get();
	UAbilitySystemComponent* Target = BoundAbilitySystem.Get();
	if (!Attacker || !Target)
	{
		return;
	}
	FVeyraRawDamageEvent Damage;
	Damage.Components.Add({ Type, Amount });
	Damage.Delivery = EVeyraDamageDelivery::Periodic;
	VeyraCombat::DealDamage(*Attacker, *Target, Damage);
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
				 UVeyraStatusEffect::TenacityMultiplierName, UVeyraStatusEffect::IncomingDamageMultiplierName, UVeyraStatusEffect::DisplacementMultiplierName,
				 UVeyraStatusEffect::HealthRegenMultiplierName, UVeyraStatusEffect::OutgoingDamageMultiplierName,
				 UVeyraStatusEffect::MagicResistRetainedMultiplierName })
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

bool UVeyraStatusComponent::DecayOneStack(int32 Sequence, const FActiveGameplayEffect& Ended)
{
	FServerEntry* Server = ServerEntries.Find(Sequence);
	FVeyraStatusEntry* Entry = Ledger.Entries.FindByPredicate([Sequence](const FVeyraStatusEntry& Candidate) { return Candidate.Sequence == Sequence; });
	UAbilitySystemComponent* Target = BoundAbilitySystem.Get();
	UAbilitySystemComponent* Source = Server ? Server->Source.Get() : nullptr;
	const UWorld* World = GetWorld();
	if (!Server || !Entry || !Target || !Source || !World || Server->StackDecaySeconds <= 0.0 || Entry->Stacks <= 1)
	{
		return false;
	}
	// Only a status whose time ran out decays; one removed early ends at once.
	constexpr double ExpiryTolerance = 1e-3;
	if (Ended.GetEndTime() > World->GetTimeSeconds() + ExpiryTolerance)
	{
		return false;
	}
	const FActiveGameplayEffectHandle Next = ApplyEffect(*Source, *Target, Entry->Kind, Entry->Magnitude, Entry->Stacks - 1, Server->StackDecaySeconds);
	if (!Next.IsValid())
	{
		return false;
	}
	Server->Effect = Next;
	Entry->Stacks -= 1;
	Entry->StartedAt = GetServerNow();
	Entry->EndsAt = Entry->StartedAt + Server->StackDecaySeconds;
	MarkLedgerChanged();
	return true;
}
