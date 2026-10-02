// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Echoes/VeyraEchoSubsystem.h"

#include "AbilitySystemComponent.h"
#include "Casting/VeyraCastSubsystem.h"
#include "Echoes/VeyraEcho.h"
#include "Echoes/VeyraEchoRules.h"
#include "Engine/World.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraAbilitiesLog.h"
#include "VeyraAbilitiesVerbs.h"
#include "VeyraCombatVerbs.h"

namespace
{
	/** Ability's projection, when it projects an Echo. */
	const FVeyraEchoProjectionTuning* ProjectionOf(const FVeyraContentId& Ability)
	{
		const FVeyraEchoAbilityTuning* Tuning = UVeyraAbilitiesTuningSubsystem::FindEcho(Ability);
		return Tuning && Tuning->Projection.Num() == 1 ? &Tuning->Projection[0] : nullptr;
	}
}

void UVeyraEchoSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UVeyraCombatEventSubsystem* Events = Collection.InitializeDependency<UVeyraCombatEventSubsystem>())
	{
		HostileDamageHandle = Events->OnHostileDamage.AddUObject(this, &UVeyraEchoSubsystem::OnHostileDamage);
	}
}

void UVeyraEchoSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (UVeyraCombatEventSubsystem* Events = World->GetSubsystem<UVeyraCombatEventSubsystem>())
		{
			Events->OnHostileDamage.Remove(HostileDamageHandle);
		}
		for (FKept& Entry : Kept)
		{
			World->GetTimerManager().ClearTimer(Entry.Timer);
			World->GetTimerManager().ClearTimer(Entry.ControlTimer);
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

AVeyraEcho* UVeyraEchoSubsystem::FindCommanded(const UAbilitySystemComponent& Holder) const
{
	const FKept* Entry = FindKept(Holder);
	return Entry && Entry->bCommanded ? FindStanding(Holder) : nullptr;
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
	Echo->SetRepeatsLeft(Entry.RepeatsLeft);
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

AVeyraEcho* UVeyraEchoSubsystem::Project(UAbilitySystemComponent& Holder, const FVeyraContentId& Ability, const FVector& Where)
{
	const FVeyraEchoProjectionTuning* Projection = ProjectionOf(Ability);
	UWorld* World = GetWorld();
	if (!Projection || !World)
	{
		return nullptr;
	}
	AVeyraEcho* Echo = Form(Holder, Ability, Where, Projection->Integrity);
	FKept* Entry = Echo ? FindKept(Holder) : nullptr;
	if (!Entry)
	{
		return nullptr;
	}
	const double Now = World->GetTimeSeconds();
	Entry->bProjected = true;
	Entry->Stasis = Projection->Stasis;
	Entry->ImmuneUntil = Now + Projection->ImmunitySeconds;
	Entry->Integrity = Projection->Integrity;
	Entry->UpdatedAt = Now;
	Echo->SetProjection(Entry->ImmuneUntil, Now + Projection->FormationSeconds);
	ApplyIntegrity(*Entry);

	// Its holder waits in Stasis where it stands, the anchor of the tether (Combat Bible §10's controllable proxy).
	if (const TOptional<FVeyraStatusSpec> Stasis = UVeyraAbilitiesTuningSubsystem::FindStatus(Projection->Stasis))
	{
		VeyraCombat::ApplyStatus(Holder, Holder, Stasis.GetValue());
	}
	const TWeakObjectPtr<UAbilitySystemComponent> Weak(&Holder);
	World->GetTimerManager().SetTimer(Entry->Timer, FTimerDelegate::CreateUObject(this, &UVeyraEchoSubsystem::OnProjectionUpdate, Weak),
		static_cast<float>(Projection->UpdateSeconds), /*bLoop*/ true);
	if (Projection->FormationSeconds > 0.0)
	{
		World->GetTimerManager().SetTimer(Entry->ControlTimer, FTimerDelegate::CreateUObject(this, &UVeyraEchoSubsystem::OnFormed, Weak),
			static_cast<float>(Projection->FormationSeconds), /*bLoop*/ false);
	}
	else
	{
		OnFormed(Weak);
	}
	return Echo;
}

void UVeyraEchoSubsystem::OnFormed(TWeakObjectPtr<UAbilitySystemComponent> Holder)
{
	UAbilitySystemComponent* Projector = Holder.Get();
	FKept* Entry = Projector ? FindKept(*Projector) : nullptr;
	AVeyraEcho* Echo = Entry ? FindStanding(*Projector) : nullptr;
	if (!Echo || Entry->bCommanded)
	{
		return;
	}
	// Formed, it takes its holder's orders from now on (ADR-050 §6).
	Entry->bCommanded = true;
	UE_LOG(LogVeyraAbilities, Log, TEXT("%s takes control from %s."), *GetNameSafe(Echo), *GetNameSafe(Projector->GetOwner()));
	OnEchoCommanded.Broadcast(*Projector, *Echo);
}

void UVeyraEchoSubsystem::OnProjectionUpdate(TWeakObjectPtr<UAbilitySystemComponent> Holder)
{
	const UAbilitySystemComponent* Projector = Holder.Get();
	FKept* Entry = Projector ? FindKept(*Projector) : nullptr;
	const FVeyraEchoProjectionTuning* Projection = Entry && Entry->Echo.IsValid() ? ProjectionOf(Entry->Echo->GetAbility()) : nullptr;
	UWorld* World = GetWorld();
	if (!Entry || !Projection || !World || !FindStanding(*Projector))
	{
		return;
	}
	// It decays from the cast to its end, through its immunity too: its hard lifetime (Item Bible §11).
	const double Now = World->GetTimeSeconds();
	Entry->Integrity -= Projection->DecayPerSecond * (Now - Entry->UpdatedAt);
	Entry->UpdatedAt = Now;
	ApplyIntegrity(*Entry);
}

void UVeyraEchoSubsystem::ApplyIntegrity(FKept& Entry)
{
	AVeyraEcho* Echo = Entry.Echo.Get();
	const FVeyraEchoProjectionTuning* Projection = Echo ? ProjectionOf(Echo->GetAbility()) : nullptr;
	if (!Echo || !Projection || Echo->IsWithdrawn())
	{
		return;
	}
	const double Radius = VeyraEchoRules::TetherRadius(*Projection, Entry.Integrity);
	Echo->SetIntegrity(Entry.Integrity, Radius);
	// The circle is a break threshold, not a wall: beyond it, or once it shrinks past the Echo, the tether snaps.
	if (Entry.Integrity <= 0.0)
	{
		EndKept(Entry, EVeyraEchoEnd::Faded);
	}
	else if (FVector::Dist2D(Echo->GetActorLocation(), Echo->GetAnchor()) > Radius)
	{
		EndKept(Entry, EVeyraEchoEnd::Strayed);
	}
}

void UVeyraEchoSubsystem::OnHostileDamage(const FVeyraHostileDamageEvent& Event)
{
	const UAbilitySystemComponent* Target = Event.Target.Get();
	const AVeyraEcho* Struck = Target ? Cast<AVeyraEcho>(Target->GetOwner()) : nullptr;
	const UAbilitySystemComponent* Holder = Struck ? Struck->GetOwnerAbilities() : nullptr;
	FKept* Entry = Holder ? FindKept(*Holder) : nullptr;
	const FVeyraEchoProjectionTuning* Projection = Struck ? ProjectionOf(Struck->GetAbility()) : nullptr;
	UWorld* World = GetWorld();
	if (!Entry || !Entry->bProjected || Entry->Echo.Get() != Struck || !Projection || !World || World->GetTimeSeconds() < Entry->ImmuneUntil)
	{
		return;
	}
	const UAbilitySystemComponent* Source = Event.Source.Get();
	const bool bFromVanguard = Source && VeyraUnits::IsVanguard(Source->GetOwner());
	Entry->Integrity -= VeyraEchoRules::IntegrityLoss(Projection->IntegrityLoss, Event.Delivery, bFromVanguard);
	ApplyIntegrity(*Entry);
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
	Echo->SetRepeatsLeft(--Entry->RepeatsLeft);
	if (Entry->RepeatsLeft == 0)
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

EVeyraCastRejection UVeyraEchoSubsystem::CastFrom(UAbilitySystemComponent& Holder, EVeyraAbilitySlot Slot, const FVeyraCastTarget& Target)
{
	FKept* Entry = FindKept(Holder);
	AVeyraEcho* Echo = FindCommanded(Holder);
	const FVeyraEchoAbilityTuning* Tuning = Echo ? UVeyraAbilitiesTuningSubsystem::FindEcho(Echo->GetAbility()) : nullptr;
	UVeyraCastSubsystem* Casts = GetWorld() ? GetWorld()->GetSubsystem<UVeyraCastSubsystem>() : nullptr;
	// One of its holder's eligible abilities, once: never an item's Active or a Flux Spell (Item Bible §11).
	if (!Entry || !Echo || !Tuning || !Casts || !Tuning->Slots.Contains(Slot) || Entry->RepeatsLeft <= 0)
	{
		return EVeyraCastRejection::Projected;
	}
	const AActor* Participant = Holder.GetOwner();
	const UVeyraAbilityLoadoutComponent* Loadout = Participant ? Participant->FindComponentByClass<UVeyraAbilityLoadoutComponent>() : nullptr;
	const FVeyraLoadoutEntry* Held = Loadout ? Loadout->FindSlot(Slot) : nullptr;
	if (!Held)
	{
		return EVeyraCastRejection::UnknownAbility;
	}
	const int32 Rank = VeyraAbilities::RankOf(Holder, Held->Ability);
	if (Rank < 1)
	{
		return EVeyraCastRejection::NotLearned;
	}
	const FGameplayAbilitySpec* Spec = Holder.FindAbilitySpecFromHandle(Held->Handle);
	UVeyraGameplayAbility* Instance = Spec ? Cast<UVeyraGameplayAbility>(Spec->GetPrimaryInstance()) : nullptr;
	if (!Instance || !Instance->CanReverberate(Held->Ability))
	{
		return EVeyraCastRejection::Projected;
	}
	// The order's point or unit must be one the ability may take, as for any cast, before its repeat is spent.
	if (const EVeyraCastRejection Refusal = Instance->CheckRepeatTarget(*Echo, Held->Ability, Target); Refusal != EVeyraCastRejection::None)
	{
		return Refusal;
	}
	// Aimed from where the Echo stands, at the order's point or unit, within the ability's range.
	const FVector At = Echo->GetActorLocation();
	AActor* Aimed = Target.Actor.Get();
	const FVector Point = Aimed ? Aimed->GetActorLocation() : Target.bHasLocation ? Target.Location : At;
	const FVeyraCastTuning* CastTuning = VeyraAbilityRules::FindCast(UVeyraAbilitiesTuningSubsystem::Get(), Held->Ability);
	const FVeyraEchoAim Aim = VeyraEchoRules::AimFrom(At, At, Point, Echo->GetActorForwardVector().GetSafeNormal2D(), CastTuning ? CastTuning->CastRange : 0.0);
	FVeyraCast Repeat;
	Repeat.Ability = Held->Ability;
	Repeat.Rank = Rank;
	Repeat.CastId = Casts->IssueCastId();
	Repeat.Caster = Echo->GetAbilitySystemComponent();
	Repeat.TargetActor = Aimed;
	Repeat.CasterLocation = At;
	Repeat.Point = Aim.Point;
	Repeat.Direction = Aim.Direction;
	Echo->RefreshOffence();
	Echo->SetRepeatsLeft(--Entry->RepeatsLeft);
	UE_LOG(LogVeyraAbilities, Log, TEXT("%s casts %s (cast %d)."), *GetNameSafe(Echo), *Held->Ability.ToString(), Repeat.CastId);
	Instance->DeliverRepeat(Repeat);
	return EVeyraCastRejection::None;
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
		World->GetTimerManager().ClearTimer(Entry.ControlTimer);
	}
	if (!Echo || Echo->IsWithdrawn())
	{
		return;
	}
	Echo->Withdraw();
	Entry.bCommanded = false;
	// Its end ends its holder's Stasis where it began: the effect that applied it says so (Combat Bible §10).
	if (UAbilitySystemComponent* Holder = Entry.Holder.Get(); Holder && Entry.bProjected)
	{
		VeyraCombat::RemoveStatus(*Holder, Entry.Stasis);
	}
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
