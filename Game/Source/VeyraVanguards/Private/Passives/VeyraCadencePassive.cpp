// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Passives/VeyraCadencePassive.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attacks/VeyraBasicAttackComponent.h"
#include "Delivery/VeyraAreaDelivery.h"
#include "Delivery/VeyraEffectDelivery.h"
#include "Delivery/VeyraProjectile.h"
#include "Engine/World.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Statuses/VeyraStatusComponent.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "VeyraVanguardsLog.h"

namespace VeyraCadence
{
	// A passive has no ranks; its one amounts apply at every level.
	constexpr int32 PassiveRank = 1;

	const UVeyraStatusComponent* StatusesOf(const UAbilitySystemComponent* AbilitySystem)
	{
		const AActor* Owner = AbilitySystem ? AbilitySystem->GetOwner() : nullptr;
		return Owner ? Owner->FindComponentByClass<UVeyraStatusComponent>() : nullptr;
	}
}

void UVeyraCadencePassive::Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId)
{
	Super::Start(Owner, InPassiveId);
	AActor* Participant = Owner.GetOwner();
	if (UVeyraBasicAttackComponent* Component = Participant ? Participant->FindComponentByClass<UVeyraBasicAttackComponent>() : nullptr)
	{
		Attacks = Component;
		AttackHandle = Component->OnAttack.AddUObject(this, &UVeyraCadencePassive::OnAttack);
		ModifyHandle = Component->OnModifyAttack.AddUObject(this, &UVeyraCadencePassive::OnModifyAttack);
	}
	if (UVeyraCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		StatusHandle = Events->OnStatusApplied.AddUObject(this, &UVeyraCadencePassive::OnStatusApplied);
	}
}

void UVeyraCadencePassive::Stop()
{
	if (UVeyraBasicAttackComponent* Component = Attacks.Get())
	{
		Component->OnAttack.Remove(AttackHandle);
		Component->OnModifyAttack.Remove(ModifyHandle);
	}
	if (UVeyraCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		Events->OnStatusApplied.Remove(StatusHandle);
	}
	AttackHandle.Reset();
	ModifyHandle.Reset();
	StatusHandle.Reset();
	Attacks.Reset();
	Super::Stop();
}

int32 UVeyraCadencePassive::GetStacks() const
{
	const FVeyraCadenceTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindCadence(PassiveId);
	const UVeyraStatusComponent* Statuses = VeyraCadence::StatusesOf(OwnerAbilitySystem.Get());
	if (!Tuning || !Statuses)
	{
		return 0;
	}
	const FVeyraStatusEntry* Entry = Statuses->GetLedger().Entries.FindByPredicate([Tuning](const FVeyraStatusEntry& Candidate) { return Candidate.Id == Tuning->Status; });
	return Entry ? Entry->Stacks : 0;
}

bool UVeyraCadencePassive::IsFull() const
{
	const FVeyraCadenceTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindCadence(PassiveId);
	const TOptional<FVeyraStatusSpec> Stacks = Tuning ? UVeyraAbilitiesTuningSubsystem::FindStatus(Tuning->Status) : TOptional<FVeyraStatusSpec>();
	return Stacks.IsSet() && (GetStacks() >= Stacks->MaxStacks || EndsAt(Tuning->FullStatus) > 0.0);
}

double UVeyraCadencePassive::EndsAt(const FVeyraContentId& Id) const
{
	const UVeyraStatusComponent* Statuses = VeyraCadence::StatusesOf(OwnerAbilitySystem.Get());
	const FVeyraStatusEntry* Entry = Statuses ? Statuses->GetLedger().Entries.FindByPredicate([&Id](const FVeyraStatusEntry& Candidate) { return Candidate.Id == Id; }) : nullptr;
	return Entry ? Entry->EndsAt : 0.0;
}

void UVeyraCadencePassive::AddStacks(int32 Count, double MinSeconds)
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const FVeyraCadenceTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindCadence(PassiveId);
	TOptional<FVeyraStatusSpec> Stack = Tuning ? UVeyraAbilitiesTuningSubsystem::FindStatus(Tuning->Status) : TOptional<FVeyraStatusSpec>();
	if (!Owner || !Stack.IsSet())
	{
		return;
	}
	// Dug in, it decays more slowly (§7).
	if (EndsAt(Tuning->SteadyStatus) > 0.0)
	{
		Stack->StackDecaySeconds *= Tuning->SteadyDecayMultiplier;
	}
	Stack->DurationSeconds = FMath::Max(Stack->DurationSeconds, MinSeconds);
	for (int32 Added = 0; Added < Count; ++Added)
	{
		VeyraCombat::ApplyStatus(*Owner, *Owner, Stack.GetValue());
	}
}

void UVeyraCadencePassive::OnAttack(const FVeyraAttackEvent& Event)
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const FVeyraCadenceTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindCadence(PassiveId);
	const UWorld* World = GetWorld();
	if (!Owner || !Tuning || !World || Event.Attacker.Get() != Owner)
	{
		return;
	}
	// Held full by The Last Volley, a stack lasts as long as it does.
	const double FullUntil = EndsAt(Tuning->FullStatus);
	const double Held = FullUntil > 0.0 ? FullUntil - World->GetTimeSeconds() : 0.0;
	const UAbilitySystemComponent* Target = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Event.Target.Get());
	const UVeyraStatusComponent* Marks = VeyraCadence::StatusesOf(Target);
	const bool bRanged = Marks && Marks->HasFrom(Tuning->ExtraStackOn, *Owner);
	AddStacks(bRanged ? 2 : 1, Held);

	if (FullUntil > 0.0 && ++VolleyAttacks % Tuning->SpectralRank.EveryAttacks == 0)
	{
		if (AActor* Struck = Event.Target.Get())
		{
			FireSpectralRank(*Struck);
		}
	}
}

void UVeyraCadencePassive::OnModifyAttack(FVeyraAttackPlan& Plan)
{
	const UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const FVeyraCadenceTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindCadence(PassiveId);
	UWorld* World = GetWorld();
	if (!Owner || !Tuning || !World || !IsFull() || !Plan.Target.IsValid())
	{
		return;
	}
	// Firing Line: the echo repeats the attack's own damage in part, a moment later (§7).
	double Own = 0.0;
	for (const FVeyraDamageComponent& Component : Plan.BaseDamage)
	{
		Own += Component.Amount;
	}
	const FVeyraFiringLineTuning& Echo = Tuning->FiringLine;
	const double Amount = Own * Echo.AttackDamageFraction + VeyraEffectDelivery::DamageAmount(*Owner, Echo.Damage, VeyraCadence::PassiveRank);
	FTimerHandle Timer;
	World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &UVeyraCadencePassive::LaunchEcho, Plan.Target, Amount),
		static_cast<float>(FMath::Max(Echo.DelaySeconds, UE_KINDA_SMALL_NUMBER)), /*bLoop*/ false);
}

void UVeyraCadencePassive::LaunchEcho(TWeakObjectPtr<AActor> Target, double Amount)
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const FVeyraCadenceTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindCadence(PassiveId);
	const AActor* Body = Owner ? Owner->GetAvatarActor() : nullptr;
	AActor* Struck = Target.Get();
	UWorld* World = GetWorld();
	if (!Tuning || !Body || !Struck || !World)
	{
		return;
	}
	AVeyraProjectile* Echo = World->SpawnActor<AVeyraProjectile>(AVeyraProjectile::StaticClass(), FTransform(Body->GetActorLocation()));
	if (!Echo)
	{
		UE_LOG(LogVeyraVanguards, Warning, TEXT("Firing Line could not launch its echo at %s."), *GetNameSafe(Struck));
		return;
	}
	FVeyraEffectBundleTuning Effects;
	FVeyraDamageTuning& Damage = Effects.Damage.AddDefaulted_GetRef();
	Damage.Type = Tuning->FiringLine.Damage.Type;
	Damage.AmountByRank = { Amount };
	// Proc damage: it is not an attack, so it echoes nothing and applies no On-Hit (Combat Bible §16).
	Echo->LaunchHoming(*Owner, *Struck, Tuning->FiringLine.Projectile.Speed, Tuning->FiringLine.Projectile.Radius,
		VeyraEffectDelivery::Prepare(*Owner, Effects, VeyraCadence::PassiveRank), PassiveId, 0);
}

void UVeyraCadencePassive::FireSpectralRank(AActor& Target)
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const FVeyraCadenceTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindCadence(PassiveId);
	const AActor* Body = Owner ? Owner->GetAvatarActor() : nullptr;
	UWorld* World = GetWorld();
	if (!Tuning || !Body || !World)
	{
		return;
	}
	FVeyraAreaZoneTuning Rank;
	Rank.Shape = Tuning->SpectralRank.Shape;
	Rank.Effects.Damage = Tuning->SpectralRank.Damage;
	const FVeyraAreaZoneTuning Zones[] = { Rank };
	FVeyraEffectFrame Frame;
	Frame.Origin = Body->GetActorLocation();
	Frame.Direction = (Target.GetActorLocation() - Frame.Origin).GetSafeNormal2D();
	Frame.bOriginIsCaster = true;
	VeyraAreaDelivery::Resolve(*World, *Owner, Frame, VeyraAreaDelivery::PrepareZones(*Owner, Zones, VeyraCadence::PassiveRank),
		FVeyraAbilityHitSource{ PassiveId, 0 });
}

void UVeyraCadencePassive::OnStatusApplied(const FVeyraStatusApplied& Event)
{
	const FVeyraCadenceTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindCadence(PassiveId);
	const TOptional<FVeyraStatusSpec> Stack = Tuning ? UVeyraAbilitiesTuningSubsystem::FindStatus(Tuning->Status) : TOptional<FVeyraStatusSpec>();
	const UWorld* World = GetWorld();
	if (!Tuning || !Stack.IsSet() || !World || Event.Target.Get() != OwnerAbilitySystem.Get() || Event.Id != Tuning->FullStatus)
	{
		return;
	}
	// The Last Volley: full at once, and held so while it lasts (§7).
	VolleyAttacks = 0;
	AddStacks(Stack->MaxStacks, Event.EndsAt - World->GetTimeSeconds());
}
