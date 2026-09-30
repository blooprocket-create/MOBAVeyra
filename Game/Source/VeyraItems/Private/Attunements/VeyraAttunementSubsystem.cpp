// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attunements/VeyraAttunementSubsystem.h"

#include "AbilitySystemComponent.h"
#include "Absorption/VeyraAbsorptionLedger.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Engine/World.h"
#include "Inventory/VeyraInventoryComponent.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Statuses/VeyraStatusTypes.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraItemsTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraCombatVerbs.h"
#include "VeyraItemsLog.h"

void UVeyraAttunementSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UVeyraCombatEventSubsystem* Events = Collection.InitializeDependency<UVeyraCombatEventSubsystem>())
	{
		DamageDealtHandle = Events->OnDamageDealt.AddUObject(this, &UVeyraAttunementSubsystem::OnDamageDealt);
	}
}

void UVeyraAttunementSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (UVeyraCombatEventSubsystem* Events = World->GetSubsystem<UVeyraCombatEventSubsystem>())
		{
			Events->OnDamageDealt.Remove(DamageDealtHandle);
		}
	}
	Super::Deinitialize();
}

void UVeyraAttunementSubsystem::OnDamageDealt(const FVeyraDamageDealtEvent& Event)
{
	UAbilitySystemComponent* Holder = Event.Source.Get();
	UAbilitySystemComponent* Target = Event.Target.Get();
	const AActor* Participant = Holder ? Holder->GetOwner() : nullptr;
	const UVeyraInventoryComponent* Inventory = Participant ? Participant->FindComponentByClass<UVeyraInventoryComponent>() : nullptr;
	// Each acts on damage that reached an enemy Vanguard (Item Bible §8–§9).
	if (!Inventory || !Target || !Participant->HasAuthority() || !VeyraUnits::IsVanguard(Target->GetOwner()) || Event.Total() <= 0.0)
	{
		return;
	}
	const FVeyraItemsTuning& Tuning = UVeyraItemsTuningSubsystem::Get();
	TArray<FVeyraContentId, TInlineAllocator<6>> Held;
	for (const FVeyraInventorySlot& Slot : Inventory->GetSlots())
	{
		if (const FVeyraItemDefinition* Item = Slot.IsEmpty() ? nullptr : Tuning.Items.Find(Slot.Item))
		{
			for (const FVeyraContentId& Attunement : Item->Attunement)
			{
				Held.AddUnique(Attunement);
			}
		}
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const auto Over = [Now](const FTimed& Entry) { return Entry.Until <= Now || !Entry.Holder.IsValid(); };
	Cooldowns.RemoveAllSwap(Over);
	Primes.RemoveAllSwap(Over);
	// Held is this hit's own copy: a proc one of these deals sends an event of its own through here.
	for (const FVeyraContentId& Attunement : Held)
	{
		if (Tuning.ReprisalGuard.Contains(Attunement))
		{
			ReprisalGuard(Attunement, Event, *Holder, Now);
		}
		else if (Tuning.Drag.Contains(Attunement))
		{
			Drag(Attunement, Event, *Holder, *Target);
		}
		else if (Tuning.Convergence.Contains(Attunement))
		{
			Convergence(Attunement, Event, *Holder, *Target, Now);
		}
		else if (Tuning.Fracture.Contains(Attunement))
		{
			Fracture(Attunement, Event, *Holder, *Target);
		}
	}
}

void UVeyraAttunementSubsystem::ReprisalGuard(const FVeyraContentId& Attunement, const FVeyraDamageDealtEvent& Event, UAbilitySystemComponent& Holder, double Now)
{
	// The triggering attack or ability (Item Bible §8): not a proc a hit spreads, nor a tick.
	const bool bTriggers = Event.Delivery == EVeyraDamageDelivery::BasicAttack || Event.Delivery == EVeyraDamageDelivery::Ability;
	const bool bCooling = Cooldowns.ContainsByPredicate([&Holder, &Attunement](const FTimed& Entry) {
		return Entry.Holder.Get() == &Holder && Entry.Attunement == Attunement;
	});
	if (!bTriggers || bCooling || !VeyraTargeting::IsAlive(Holder.GetOwner()))
	{
		return;
	}
	const FVeyraReprisalGuardTuning& Guard = UVeyraItemsTuningSubsystem::Get().ReprisalGuard.FindChecked(Attunement);
	FVeyraShieldGrant Grant;
	Grant.Id = Attunement;
	Grant.Amount = FMath::Min(Event.Total() * Guard.DamageFraction, Guard.MaxShield);
	Grant.MaxAmount = Guard.MaxShield;
	Grant.DurationSeconds = Guard.ShieldSeconds;
	if (!VeyraCombat::GrantShield(Holder, Holder, Grant).IsValid())
	{
		return;
	}
	// Then the Attunement cools down.
	FTimed& Cooling = Cooldowns.AddDefaulted_GetRef();
	Cooling.Holder = &Holder;
	Cooling.Attunement = Attunement;
	Cooling.Until = Now + Guard.CooldownSeconds;
	UE_LOG(LogVeyraItems, Verbose, TEXT("%s's %s shields it for %g."), *GetNameSafe(Holder.GetOwner()), *Attunement.ToString(), Grant.Amount);
}

void UVeyraAttunementSubsystem::Drag(const FVeyraContentId& Attunement, const FVeyraDamageDealtEvent& Event, UAbilitySystemComponent& Holder,
	UAbilitySystemComponent& Target)
{
	// Damaging abilities briefly slow (Item Bible §9).
	if (Event.Delivery != EVeyraDamageDelivery::Ability || !VeyraTargeting::IsAlive(Target.GetOwner()))
	{
		return;
	}
	const FVeyraDragTuning& Tuning = UVeyraItemsTuningSubsystem::Get().Drag.FindChecked(Attunement);
	FVeyraStatusSpec Slow;
	Slow.Id = Attunement;
	Slow.Kind = EVeyraStatusKind::Slow;
	Slow.Stacking = EVeyraStackingPolicy::UniqueRefresh;
	Slow.Magnitude = Tuning.Slow;
	Slow.DurationSeconds = Tuning.DurationSeconds;
	VeyraCombat::ApplyStatus(Holder, Target, Slow);
}

void UVeyraAttunementSubsystem::Convergence(const FVeyraContentId& Attunement, const FVeyraDamageDealtEvent& Event, UAbilitySystemComponent& Holder,
	UAbilitySystemComponent& Target, double Now)
{
	// One damaging ability primes the target; the holder's next one on it within the window consumes
	// the prime for bonus magic damage (Item Bible §9).
	if (Event.Delivery != EVeyraDamageDelivery::Ability || !VeyraTargeting::IsAlive(Target.GetOwner()))
	{
		return;
	}
	const FVeyraConvergenceTuning& Tuning = UVeyraItemsTuningSubsystem::Get().Convergence.FindChecked(Attunement);
	const int32 Primed = Primes.IndexOfByPredicate([&Holder, &Target, &Attunement](const FTimed& Entry) {
		return Entry.Holder.Get() == &Holder && Entry.Target.Get() == &Target && Entry.Attunement == Attunement;
	});
	if (Primed == INDEX_NONE)
	{
		FTimed& Prime = Primes.AddDefaulted_GetRef();
		Prime.Holder = &Holder;
		Prime.Target = &Target;
		Prime.Attunement = Attunement;
		Prime.Until = Now + Tuning.WindowSeconds;
		return;
	}
	Primes.RemoveAtSwap(Primed);
	// A proc, which neither primes nor consumes (ADR-022 §3).
	FVeyraRawDamageEvent Bonus;
	Bonus.Components.Add({ EVeyraDamageType::Magic, Tuning.BaseDamage + Tuning.MagicPowerRatio * Holder.GetNumericAttribute(UVeyraOffenceSet::GetMagicPowerAttribute()) });
	Bonus.Delivery = EVeyraDamageDelivery::Proc;
	VeyraCombat::DealDamage(Holder, Target, Bonus);
}

void UVeyraAttunementSubsystem::Fracture(const FVeyraContentId& Attunement, const FVeyraDamageDealtEvent& Event, UAbilitySystemComponent& Holder,
	UAbilitySystemComponent& Target)
{
	// Repeated magic damage progressively reduces Magic Resistance, up to a cap (Item Bible §9): each
	// instance with some adds a stack, and the stacks refresh and end together.
	if (Event.Of(EVeyraDamageType::Magic) <= 0.0 || !VeyraTargeting::IsAlive(Target.GetOwner()))
	{
		return;
	}
	const FVeyraStackingAttunementTuning& Tuning = UVeyraItemsTuningSubsystem::Get().Fracture.FindChecked(Attunement);
	FVeyraStatusSpec Shred;
	Shred.Id = Attunement;
	Shred.Kind = EVeyraStatusKind::MagicResistReduction;
	Shred.Stacking = EVeyraStackingPolicy::Stacking;
	Shred.Magnitude = Tuning.PerStack;
	Shred.MaxStacks = Tuning.MaxStacks;
	Shred.DurationSeconds = Tuning.DurationSeconds;
	VeyraCombat::ApplyStatus(Holder, Target, Shred);
}
