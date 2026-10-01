// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Companions/VeyraCompanionSubsystem.h"

#include "AbilitySystemComponent.h"
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
	}
}

void UVeyraCompanionSubsystem::Deinitialize()
{
	UWorld* World = GetWorld();
	if (UVeyraCombatEventSubsystem* Events = World ? World->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		Events->OnDeath.Remove(DeathHandle);
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
	// On world time, so a pause holds it (ADR-006 §8).
	const TWeakObjectPtr<UAbilitySystemComponent> WeakOwner = &Owner;
	World->GetTimerManager().SetTimer(Entry.Timer, FTimerDelegate::CreateWeakLambda(this, [this, WeakOwner] {
		if (const UAbilitySystemComponent* Present = WeakOwner.Get())
		{
			Keep(*Present);
		}
	}), static_cast<float>(Tuning->ThinkSeconds), /*bLoop*/ true);
	Keep(Owner);
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
	const AActor* OwnerBody = Keeper->GetAvatarActor();
	const bool bOwnerAlive = VeyraTargeting::IsAlive(OwnerBody);
	AVeyraCompanion* Companion = Entry->Companion.Get();
	if (!Companion)
	{
		if (bOwnerAlive)
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
		Banish(*Entry);
		return;
	}
	if (Companion->IsBanished() && GetWorld()->GetTimeSeconds() >= Entry->ReformsAt)
	{
		const FVeyraCompanionTuning* Tuning = Companion->GetDefinition();
		Companion->Reform(BesideOwner(*OwnerBody, Tuning ? Tuning->CapsuleRadius : 0.0));
		UE_LOG(LogVeyraAbilities, Log, TEXT("%s reformed beside %s."), *GetNameSafe(Companion), *GetNameSafe(OwnerBody));
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

void UVeyraCompanionSubsystem::OnDeath(const FVeyraDeathEvent& Death)
{
	const UAbilitySystemComponent* Victim = Death.Victim.Get();
	if (!Victim)
	{
		return;
	}
	for (FKept& Entry : Kept)
	{
		AVeyraCompanion* Companion = Entry.Companion.Get();
		if (Companion && Victim == Companion->GetAbilitySystemComponent())
		{
			// Killed, it reforms once its time has passed and its owner lives (Roster Bible §10).
			const FVeyraCompanionTuning* Tuning = Companion->GetDefinition();
			Entry.ReformsAt = Death.DiedAtSeconds + (Tuning ? Tuning->ReformSeconds : 0.0);
			Banish(Entry);
		}
		else if (Victim == Entry.Owner.Get())
		{
			// It leaves with its owner, and returns as its owner revives (ADR-034 §11).
			Banish(Entry);
		}
	}
}

FVector UVeyraCompanionSubsystem::BesideOwner(const AActor& OwnerBody, double Radius) const
{
	// Behind its owner, the two bodies' edges touching, on the nearest ground there.
	const FVector Behind = OwnerBody.GetActorLocation() - OwnerBody.GetActorForwardVector() * (OwnerBody.GetSimpleCollisionRadius() + Radius);
	return VeyraCombat::NearestGround(*GetWorld(), Behind);
}
