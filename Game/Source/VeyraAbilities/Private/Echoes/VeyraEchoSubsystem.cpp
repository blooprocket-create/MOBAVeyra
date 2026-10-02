// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Echoes/VeyraEchoSubsystem.h"

#include "AbilitySystemComponent.h"
#include "Casting/VeyraCastSubsystem.h"
#include "Echoes/VeyraEcho.h"
#include "Echoes/VeyraEchoRules.h"
#include "Engine/World.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraAbilitiesLog.h"
#include "VeyraCombatVerbs.h"

void UVeyraEchoSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		for (FKept& Entry : Kept)
		{
			World->GetTimerManager().ClearTimer(Entry.Timer);
		}
	}
	Kept.Reset();
	Super::Deinitialize();
}

UVeyraEchoSubsystem::FKept* UVeyraEchoSubsystem::FindKept(const UAbilitySystemComponent& Holder)
{
	return Kept.FindByPredicate([&Holder](const FKept& Each) { return Each.Holder.Get() == &Holder; });
}

const UVeyraEchoSubsystem::FKept* UVeyraEchoSubsystem::FindKept(const UAbilitySystemComponent& Holder) const
{
	return Kept.FindByPredicate([&Holder](const FKept& Each) { return Each.Holder.Get() == &Holder; });
}

AVeyraEcho* UVeyraEchoSubsystem::FindStanding(const UAbilitySystemComponent& Holder) const
{
	const FKept* Entry = FindKept(Holder);
	AVeyraEcho* Echo = Entry ? Entry->Echo.Get() : nullptr;
	return Echo && !Echo->IsWithdrawn() ? Echo : nullptr;
}

AVeyraEcho* UVeyraEchoSubsystem::Form(UAbilitySystemComponent& Holder, const FVeyraContentId& Ability, const FVector& Where, TOptional<double> Integrity)
{
	UWorld* World = GetWorld();
	const FVeyraEchoAbilityTuning* Tuning = UVeyraAbilitiesTuningSubsystem::FindEcho(Ability);
	const AActor* HolderBody = Holder.GetAvatarActor();
	if (!World || !Tuning || !HolderBody)
	{
		UE_LOG(LogVeyraAbilities, Error, TEXT("%s cannot form an Echo for %s: Abilities.json defines none, or it has no body."),
			*GetNameSafe(Holder.GetOwner()), *Ability.ToString());
		return nullptr;
	}
	// One at a time: the last ends, and goes now that nothing it set going can still need it.
	if (FKept* Existing = FindKept(Holder))
	{
		EndKept(*Existing, EVeyraEchoEnd::Replaced);
		if (AVeyraEcho* Old = Existing->Echo.Get())
		{
			Old->Destroy();
		}
		Kept.RemoveAll([&Holder](const FKept& Each) { return Each.Holder.Get() == &Holder; });
	}
	const FVector Ground = VeyraCombat::NearestGround(*World, Where);
	FVector Facing = Ground - HolderBody->GetActorLocation();
	Facing.Z = 0.0;
	const FTransform Placed(Facing.IsNearlyZero() ? HolderBody->GetActorRotation() : Facing.Rotation(), Ground);
	AVeyraEcho* Echo = World->SpawnActorDeferred<AVeyraEcho>(AVeyraEcho::StaticClass(), Placed, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Echo)
	{
		return nullptr;
	}
	Echo->Configure(Holder, Ability, HolderBody->GetActorLocation());
	Echo->FinishSpawning(Placed);
	if (!Echo->TakeHolderSnapshot(Tuning->DamageCoefficient, Integrity))
	{
		UE_LOG(LogVeyraAbilities, Error, TEXT("%s's Echo could not take its holder's stats; it does not form."), *GetNameSafe(Holder.GetOwner()));
		Echo->Destroy();
		return nullptr;
	}
	FKept& Entry = Kept.AddDefaulted_GetRef();
	Entry.Holder = &Holder;
	Entry.Echo = Echo;
	Entry.RepeatsLeft = Tuning->Repeats;
	UE_LOG(LogVeyraAbilities, Log, TEXT("%s formed for %s at %s."), *GetNameSafe(Echo), *GetNameSafe(Holder.GetOwner()), *Ground.ToCompactString());
	return Echo;
}

AVeyraEcho* UVeyraEchoSubsystem::Manifest(UAbilitySystemComponent& Holder, const FVeyraContentId& Ability, const FVector& Where)
{
	const FVeyraEchoAbilityTuning* Tuning = UVeyraAbilitiesTuningSubsystem::FindEcho(Ability);
	UWorld* World = GetWorld();
	if (!Tuning || Tuning->Manifest.Num() != 1 || !World)
	{
		return nullptr;
	}
	AVeyraEcho* Echo = Form(Holder, Ability, Where, {});
	FKept* Entry = Echo ? FindKept(Holder) : nullptr;
	if (!Entry)
	{
		return nullptr;
	}
	// What it holds while it waits, such as being Untargetable: it is there to repeat, not to be fought.
	const FVeyraEchoManifestTuning& Manifest = Tuning->Manifest[0];
	UAbilitySystemComponent& Abilities = *Echo->GetAbilitySystemComponent();
	for (const FVeyraContentId& StatusId : Manifest.Statuses)
	{
		if (const TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(StatusId))
		{
			VeyraCombat::ApplyStatus(Abilities, Abilities, Status.GetValue());
		}
	}
	World->GetTimerManager().SetTimer(Entry->Timer,
		FTimerDelegate::CreateUObject(this, &UVeyraEchoSubsystem::OnWindowEnded, TWeakObjectPtr<UAbilitySystemComponent>(&Holder)),
		static_cast<float>(Manifest.WindowSeconds), /*bLoop*/ false);
	return Echo;
}

TOptional<FVeyraCast> UVeyraEchoSubsystem::TakeRepeat(UAbilitySystemComponent& Holder, const FVeyraCast& Cast)
{
	FKept* Entry = FindKept(Holder);
	AVeyraEcho* Echo = Entry ? Entry->Echo.Get() : nullptr;
	const FVeyraEchoAbilityTuning* Tuning = Echo ? UVeyraAbilitiesTuningSubsystem::FindEcho(Echo->GetAbility()) : nullptr;
	// Only a waiting Echo takes its holder's own casts, and only of an ability in a slot it repeats (ADR-050 §5).
	if (!Echo || Echo->IsWithdrawn() || !Tuning || Tuning->Manifest.IsEmpty() || Entry->RepeatsLeft <= 0 || Cast.Ability == Echo->GetAbility())
	{
		return {};
	}
	const AActor* Participant = Holder.GetOwner();
	const UVeyraAbilityLoadoutComponent* Loadout = Participant ? Participant->FindComponentByClass<UVeyraAbilityLoadoutComponent>() : nullptr;
	const FVeyraLoadoutEntry* Slot = Loadout ? Loadout->FindAbility(Cast.Ability) : nullptr;
	UVeyraCastSubsystem* Casts = GetWorld() ? GetWorld()->GetSubsystem<UVeyraCastSubsystem>() : nullptr;
	if (!Slot || !Tuning->Slots.Contains(Slot->Slot) || !Casts)
	{
		return {};
	}
	// It deals its share of what its holder deals now.
	Echo->RefreshOffence();
	const FVeyraCastTuning* Repeated = VeyraAbilityRules::FindCast(UVeyraAbilitiesTuningSubsystem::Get(), Cast.Ability);
	const FVeyraEchoAim Aim = VeyraEchoRules::AimFrom(Echo->GetActorLocation(), Cast.CasterLocation, Cast.Point, Cast.Direction, Repeated ? Repeated->CastRange : 0.0);
	FVeyraCast Repeat = Cast;
	Repeat.Caster = Echo->GetAbilitySystemComponent();
	Repeat.CastId = Casts->IssueCastId();
	Repeat.CasterLocation = Echo->GetActorLocation();
	Repeat.Point = Aim.Point;
	Repeat.Direction = Aim.Direction;
	UE_LOG(LogVeyraAbilities, Log, TEXT("%s repeats %s (cast %d as %d)."), *GetNameSafe(Echo), *Cast.Ability.ToString(), Cast.CastId, Repeat.CastId);
	// Its last repeat ends it once the repeat is delivered: an Echo withdrawn first would deliver from the dead.
	if (--Entry->RepeatsLeft == 0)
	{
		GetWorld()->GetTimerManager().ClearTimer(Entry->Timer);
		Entry->Timer = GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,
			[this, Weak = TWeakObjectPtr<UAbilitySystemComponent>(&Holder)] {
				if (UAbilitySystemComponent* Spender = Weak.Get())
				{
					End(*Spender, EVeyraEchoEnd::Spent);
				}
			}));
	}
	return Repeat;
}

void UVeyraEchoSubsystem::End(const UAbilitySystemComponent& Holder, EVeyraEchoEnd Why)
{
	if (FKept* Entry = FindKept(Holder))
	{
		EndKept(*Entry, Why);
	}
}

void UVeyraEchoSubsystem::EndKept(FKept& Entry, EVeyraEchoEnd Why)
{
	AVeyraEcho* Echo = Entry.Echo.Get();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(Entry.Timer);
	}
	if (!Echo || Echo->IsWithdrawn())
	{
		return;
	}
	Echo->Withdraw();
	UE_LOG(LogVeyraAbilities, Log, TEXT("%s ended (%s)."), *GetNameSafe(Echo), *UEnum::GetValueAsString(Why));
	OnEchoEnded.Broadcast(*Echo, Why);
}

void UVeyraEchoSubsystem::OnWindowEnded(TWeakObjectPtr<UAbilitySystemComponent> Holder)
{
	if (const UAbilitySystemComponent* Waiting = Holder.Get())
	{
		End(*Waiting, EVeyraEchoEnd::Expired);
	}
}
