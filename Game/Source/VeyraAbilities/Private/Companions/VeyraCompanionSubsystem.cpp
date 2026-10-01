// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Companions/VeyraCompanionSubsystem.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Companions/VeyraCompanion.h"
#include "Engine/World.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraAbilitiesLog.h"
#include "VeyraCombatVerbs.h"

namespace
{
	/** Owner's Level: its participant's progression, or the first Level for a unit with none. */
	int32 LevelOf(const UAbilitySystemComponent& Owner)
	{
		const AActor* Participant = Owner.GetOwner();
		const UVeyraProgressionComponent* Progression = Participant ? Participant->FindComponentByClass<UVeyraProgressionComponent>() : nullptr;
		return Progression ? Progression->GetLevel() : 1;
	}
}

void UVeyraCompanionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UVeyraCombatEventSubsystem* Events = Collection.InitializeDependency<UVeyraCombatEventSubsystem>())
	{
		DeathHandle = Events->OnDeath.AddUObject(this, &UVeyraCompanionSubsystem::OnDeath);
		DealtHandle = Events->OnDamageDealt.AddUObject(this, &UVeyraCompanionSubsystem::OnDamageDealt);
	}
}

void UVeyraCompanionSubsystem::Deinitialize()
{
	UWorld* World = GetWorld();
	if (UVeyraCombatEventSubsystem* Events = World ? World->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		Events->OnDeath.Remove(DeathHandle);
		Events->OnDamageDealt.Remove(DealtHandle);
	}
	for (FKept& Each : Kept)
	{
		if (World)
		{
			World->GetTimerManager().ClearTimer(Each.Timer);
		}
	}
	Kept.Reset();
	Super::Deinitialize();
}

bool UVeyraCompanionSubsystem::Summon(UAbilitySystemComponent& Owner, const FVeyraContentId& Id)
{
	const FVeyraCompanionTuning* Tuning = UVeyraAbilitiesTuningSubsystem::FindCompanion(Id);
	UWorld* World = GetWorld();
	if (!Tuning || !World)
	{
		UE_LOG(LogVeyraAbilities, Error, TEXT("%s cannot keep companion %s: Abilities.json defines none."), *GetNameSafe(Owner.GetOwner()), *Id.ToString());
		return false;
	}
	if (FindKept(Owner))
	{
		UE_LOG(LogVeyraAbilities, Verbose, TEXT("%s keeps a companion already; %s is not summoned."), *GetNameSafe(Owner.GetOwner()), *Id.ToString());
		return false;
	}
	FKept& Entry = Kept.AddDefaulted_GetRef();
	Entry.Owner = &Owner;
	Entry.Id = Id;
	StartKeeping(Entry, *Tuning);
	Keep(Owner);
	return true;
}

bool UVeyraCompanionSubsystem::SummonFor(UAbilitySystemComponent& Owner, const FVeyraContentId& Id, EVeyraCompanionMode Mode, AActor& Unit, double LifetimeSeconds)
{
	const FVeyraCompanionTuning* Tuning = UVeyraAbilitiesTuningSubsystem::FindCompanion(Id);
	UWorld* World = GetWorld();
	if (!Tuning || !World || !(LifetimeSeconds > 0.0))
	{
		UE_LOG(LogVeyraAbilities, Error, TEXT("%s cannot summon companion %s: Abilities.json defines none, or it would last no time."),
			*GetNameSafe(Owner.GetOwner()), *Id.ToString());
		return false;
	}
	// One at a time (ADR-034 §3): a living summon is redirected, keeping its time; a companion kept for good stays.
	if (const FKept* Existing = FindKept(Owner))
	{
		if (Existing->EndsAt <= 0.0)
		{
			return false;
		}
		if (FindLiving(Owner))
		{
			return Redirect(Owner, Mode, Unit);
		}
		Dismiss(Owner);
	}
	const APawn* OwnerBody = AVeyraCompanion::BodyOf(Owner);
	if (!OwnerBody || !VeyraTargeting::IsAlive(OwnerBody))
	{
		return false;
	}
	FKept& Entry = Kept.AddDefaulted_GetRef();
	Entry.Owner = &Owner;
	Entry.Id = Id;
	Entry.EndsAt = World->GetTimeSeconds() + LifetimeSeconds;
	// It helps its ally as it binds, then on its pulse.
	Entry.NextPulseAt = World->GetTimeSeconds();
	Form(Entry, *OwnerBody);
	AVeyraCompanion* Companion = Entry.Companion.Get();
	if (!Companion)
	{
		Kept.Pop();
		return false;
	}
	StartKeeping(Entry, *Tuning);
	Companion->Bind(Mode, Unit);
	UE_LOG(LogVeyraAbilities, Verbose, TEXT("%s summons %s for %g s, bound to %s."), *GetNameSafe(Owner.GetOwner()), *Id.ToString(), LifetimeSeconds, *GetNameSafe(&Unit));
	Keep(Owner);
	return true;
}

bool UVeyraCompanionSubsystem::Redirect(const UAbilitySystemComponent& Owner, EVeyraCompanionMode Mode, AActor& Unit)
{
	const FKept* Entry = FindKept(Owner);
	AVeyraCompanion* Companion = Entry && Entry->EndsAt > 0.0 ? FindLiving(Owner) : nullptr;
	if (!Companion)
	{
		return false;
	}
	Companion->Bind(Mode, Unit);
	UE_LOG(LogVeyraAbilities, Verbose, TEXT("%s redirected to %s."), *GetNameSafe(Companion), *GetNameSafe(&Unit));
	return true;
}

AVeyraCompanion* UVeyraCompanionSubsystem::Find(const UAbilitySystemComponent& Owner) const
{
	const FKept* Entry = FindKept(Owner);
	return Entry ? Entry->Companion.Get() : nullptr;
}

AVeyraCompanion* UVeyraCompanionSubsystem::FindLiving(const UAbilitySystemComponent& Owner) const
{
	AVeyraCompanion* Companion = Find(Owner);
	return Companion && Companion->IsAlive() && !Companion->IsBanished() ? Companion : nullptr;
}

void UVeyraCompanionSubsystem::Keep(const UAbilitySystemComponent& Owner)
{
	FKept* Entry = FindKept(Owner);
	UAbilitySystemComponent* Keeper = Entry ? Entry->Owner.Get() : nullptr;
	if (!Keeper)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const bool bSummoned = Entry->EndsAt > 0.0;
	// A summoned companion's time runs out, and it goes for good (ADR-035 §5).
	if (bSummoned && Now >= Entry->EndsAt)
	{
		Dismiss(Owner);
		return;
	}
	// It stands beside a body: before its owner has one, nothing forms, and without one it is banished.
	const APawn* OwnerBody = AVeyraCompanion::BodyOf(*Keeper);
	const bool bOwnerAlive = OwnerBody && VeyraTargeting::IsAlive(OwnerBody);
	AVeyraCompanion* Companion = Entry->Companion.Get();
	if (!Companion)
	{
		// A summon forms as it is cast, and only then.
		if (bSummoned)
		{
			Dismiss(Owner);
		}
		else if (bOwnerAlive)
		{
			Form(*Entry, *OwnerBody);
		}
		return;
	}
	// It grows with its owner, and holds its share of its owner's power as it is now (ADR-034 §3).
	Companion->GrowTo(LevelOf(*Keeper));
	Companion->Inherit(Keeper->GetNumericAttribute(UVeyraOffenceSet::GetMagicPowerAttribute()));
	if (!bOwnerAlive)
	{
		// A summon leaves with its owner for good; a companion kept for good returns as its owner revives.
		if (bSummoned)
		{
			Dismiss(Owner);
		}
		else
		{
			Banish(*Entry);
		}
		return;
	}
	if (Companion->IsBanished() && !bSummoned && Now >= Entry->ReformsAt)
	{
		const FVeyraCompanionTuning* Tuning = Companion->GetDefinition();
		Companion->Reform(BesideOwner(*OwnerBody, Tuning ? Tuning->CapsuleRadius : 0.0));
		UE_LOG(LogVeyraAbilities, Log, TEXT("%s reformed beside %s."), *GetNameSafe(Companion), *GetNameSafe(OwnerBody));
	}
	if (Companion->GetMode() == EVeyraCompanionMode::Escort && Now >= Entry->NextPulseAt && Companion->IsAlive() && !Companion->IsBanished())
	{
		Pulse(*Entry, *Companion);
	}
}

UVeyraCompanionSubsystem::FKept* UVeyraCompanionSubsystem::FindKept(const UAbilitySystemComponent& Owner)
{
	return Kept.FindByPredicate([&Owner](const FKept& Each) { return Each.Owner.Get() == &Owner; });
}

const UVeyraCompanionSubsystem::FKept* UVeyraCompanionSubsystem::FindKept(const UAbilitySystemComponent& Owner) const
{
	return Kept.FindByPredicate([&Owner](const FKept& Each) { return Each.Owner.Get() == &Owner; });
}

void UVeyraCompanionSubsystem::StartKeeping(FKept& Entry, const FVeyraCompanionTuning& Tuning)
{
	// On world time, so a pause holds it (ADR-006 §8).
	const TWeakObjectPtr<UAbilitySystemComponent> WeakOwner = Entry.Owner;
	GetWorld()->GetTimerManager().SetTimer(Entry.Timer, FTimerDelegate::CreateWeakLambda(this, [this, WeakOwner] {
		if (const UAbilitySystemComponent* Present = WeakOwner.Get())
		{
			Keep(*Present);
		}
	}), static_cast<float>(Tuning.ThinkSeconds), /*bLoop*/ true);
}

void UVeyraCompanionSubsystem::Form(FKept& Entry, const AActor& OwnerBody)
{
	UWorld* World = GetWorld();
	UAbilitySystemComponent* Owner = Entry.Owner.Get();
	if (!World || !Owner)
	{
		return;
	}
	const FVeyraCompanionTuning* Tuning = UVeyraAbilitiesTuningSubsystem::FindCompanion(Entry.Id);
	const FTransform Where(OwnerBody.GetActorRotation(), BesideOwner(OwnerBody, Tuning ? Tuning->CapsuleRadius : 0.0));
	AVeyraCompanion* Companion = World->SpawnActorDeferred<AVeyraCompanion>(AVeyraCompanion::StaticClass(), Where, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Companion)
	{
		return;
	}
	Companion->Configure(Entry.Id, *Owner);
	Companion->FinishSpawning(Where);
	if (!Companion->InitializeStats(LevelOf(*Owner)) || !Companion->Inherit(Owner->GetNumericAttribute(UVeyraOffenceSet::GetMagicPowerAttribute())))
	{
		UE_LOG(LogVeyraAbilities, Error, TEXT("Companion %s of %s could not take its stats; it does not form."), *Entry.Id.ToString(), *GetNameSafe(Owner->GetOwner()));
		Companion->Destroy();
		return;
	}
	Entry.Companion = Companion;
	UE_LOG(LogVeyraAbilities, Log, TEXT("%s formed beside %s."), *GetNameSafe(Companion), *GetNameSafe(&OwnerBody));
}

void UVeyraCompanionSubsystem::Banish(FKept& Entry)
{
	AVeyraCompanion* Companion = Entry.Companion.Get();
	if (!Companion || Companion->IsBanished())
	{
		return;
	}
	Companion->Banish();
	OnBanished.Broadcast(*Companion);
}

void UVeyraCompanionSubsystem::Dismiss(const UAbilitySystemComponent& Owner)
{
	const int32 Index = Kept.IndexOfByPredicate([&Owner](const FKept& Each) { return Each.Owner.Get() == &Owner; });
	if (Index == INDEX_NONE)
	{
		return;
	}
	FKept Entry = MoveTemp(Kept[Index]);
	Kept.RemoveAt(Index);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(Entry.Timer);
	}
	if (AVeyraCompanion* Companion = Entry.Companion.Get())
	{
		// What ends with it ends first. Its body goes on the next tick, as this may run inside its own death,
		// and its controller goes with it; until then it is banished, hidden and touches nothing.
		Banish(Entry);
		UE_LOG(LogVeyraAbilities, Verbose, TEXT("%s is gone."), *GetNameSafe(Companion));
		if (UWorld* World = GetWorld())
		{
			const TWeakObjectPtr<AVeyraCompanion> Leaving = Companion;
			World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [Leaving] {
				if (AVeyraCompanion* Body = Leaving.Get())
				{
					Body->Destroy();
				}
			}));
		}
	}
}

void UVeyraCompanionSubsystem::Pulse(FKept& Entry, AVeyraCompanion& Companion)
{
	const FVeyraCompanionTuning* Tuning = Companion.GetDefinition();
	AActor* Ally = Companion.GetBoundTo();
	UAbilitySystemComponent* AllyAbilities = Ally ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Ally) : nullptr;
	if (!Tuning || Tuning->Escort.IsEmpty() || !AllyAbilities || !VeyraTargeting::IsAlive(Ally)
		|| VeyraTargeting::EdgeToEdgeDistance(Companion, *Ally) > Tuning->LeashRange)
	{
		return;
	}
	const FVeyraEscortTuning& Escort = Tuning->Escort[0];
	Entry.NextPulseAt = GetWorld()->GetTimeSeconds() + Escort.PulseSeconds;
	// Its own heal, from its own power, which holds its share of its owner's (ADR-034 §3).
	UAbilitySystemComponent& Helper = *Companion.GetAbilitySystemComponent();
	const double Heal = Escort.HealAmount + Helper.GetNumericAttribute(UVeyraOffenceSet::GetMagicPowerAttribute()) * Escort.HealMagicPowerRatio;
	if (Heal > 0.0)
	{
		VeyraCombat::RestoreHealthFrom(Helper, *AllyAbilities, Heal);
	}
	for (const FVeyraContentId& StatusId : Escort.Statuses)
	{
		if (const TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(StatusId))
		{
			VeyraCombat::ApplyStatus(Helper, *AllyAbilities, Status.GetValue());
		}
	}
}

void UVeyraCompanionSubsystem::OnDeath(const FVeyraDeathEvent& Death)
{
	const UAbilitySystemComponent* Victim = Death.Victim.Get();
	if (!Victim)
	{
		return;
	}
	// A summon goes for good, after the loop: dismissing takes it from the list.
	TArray<TWeakObjectPtr<UAbilitySystemComponent>, TInlineAllocator<2>> Gone;
	for (FKept& Entry : Kept)
	{
		AVeyraCompanion* Companion = Entry.Companion.Get();
		const bool bSummoned = Entry.EndsAt > 0.0;
		if (Companion && Victim == Companion->GetAbilitySystemComponent())
		{
			if (bSummoned)
			{
				// Killed, a summon does not reform (ADR-035 §5).
				Gone.Add(Entry.Owner);
				continue;
			}
			// Killed, it reforms once its time has passed and its owner lives (Roster Bible §10).
			const FVeyraCompanionTuning* Tuning = Companion->GetDefinition();
			Entry.ReformsAt = Death.DiedAtSeconds + (Tuning ? Tuning->ReformSeconds : 0.0);
			Banish(Entry);
		}
		else if (Victim == Entry.Owner.Get())
		{
			// It leaves with its owner, and returns as its owner revives (ADR-034 §11); a summon does not return.
			if (bSummoned)
			{
				Gone.Add(Entry.Owner);
				continue;
			}
			Banish(Entry);
		}
	}
	for (const TWeakObjectPtr<UAbilitySystemComponent>& Owner : Gone)
	{
		if (const UAbilitySystemComponent* Present = Owner.Get())
		{
			Dismiss(*Present);
		}
	}
}

void UVeyraCompanionSubsystem::OnDamageDealt(const FVeyraDamageDealtEvent& Dealt)
{
	// A companion's basic attack carries its statuses, as the Waterling's slow (ADR-035 §5).
	UAbilitySystemComponent* Source = Dealt.Source.Get();
	UAbilitySystemComponent* Target = Dealt.Target.Get();
	if (Dealt.Delivery != EVeyraDamageDelivery::BasicAttack || !Source || !Target)
	{
		return;
	}
	for (const FKept& Entry : Kept)
	{
		const AVeyraCompanion* Companion = Entry.Companion.Get();
		const FVeyraCompanionTuning* Tuning = Companion ? Companion->GetDefinition() : nullptr;
		if (!Tuning || Companion->GetAbilitySystemComponent() != Source)
		{
			continue;
		}
		for (const FVeyraContentId& StatusId : Tuning->AttackStatuses)
		{
			if (const TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(StatusId))
			{
				VeyraCombat::ApplyStatus(*Source, *Target, Status.GetValue());
			}
		}
		return;
	}
}

FVector UVeyraCompanionSubsystem::BesideOwner(const AActor& OwnerBody, double Radius) const
{
	// Behind its owner, the two bodies' edges touching, on the nearest ground there.
	const FVector Behind = OwnerBody.GetActorLocation() - OwnerBody.GetActorForwardVector() * (OwnerBody.GetSimpleCollisionRadius() + Radius);
	return VeyraCombat::NearestGround(*GetWorld(), Behind);
}
