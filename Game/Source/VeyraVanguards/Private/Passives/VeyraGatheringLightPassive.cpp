// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Passives/VeyraGatheringLightPassive.h"

#include "AbilitySystemComponent.h"
#include "Delivery/VeyraEffectDelivery.h"
#include "Delivery/VeyraProjectile.h"
#include "Engine/World.h"
#include "Events/VeyraAbilityEvents.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraVanguardsLog.h"

namespace
{
	// A passive has no ranks; its fragment's one amount applies at every level.
	constexpr int32 GatheringLightRank = 1;
}

void UVeyraGatheringLightPassive::Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId)
{
	Super::Start(Owner, InPassiveId);
	if (UWorld* World = GetWorld())
	{
		if (UVeyraAbilityEventSubsystem* Events = World->GetSubsystem<UVeyraAbilityEventSubsystem>())
		{
			HitHandle = Events->OnAbilityHit.AddUObject(this, &UVeyraGatheringLightPassive::OnAbilityHit);
		}
		if (UVeyraCombatEventSubsystem* Combat = World->GetSubsystem<UVeyraCombatEventSubsystem>())
		{
			DeathHandle = Combat->OnDeath.AddUObject(this, &UVeyraGatheringLightPassive::OnDeath);
		}
	}
}

void UVeyraGatheringLightPassive::Stop()
{
	if (UWorld* World = GetWorld())
	{
		if (UVeyraAbilityEventSubsystem* Events = World->GetSubsystem<UVeyraAbilityEventSubsystem>())
		{
			Events->OnAbilityHit.Remove(HitHandle);
		}
		if (UVeyraCombatEventSubsystem* Combat = World->GetSubsystem<UVeyraCombatEventSubsystem>())
		{
			Combat->OnDeath.Remove(DeathHandle);
		}
	}
	HitHandle.Reset();
	DeathHandle.Reset();
	Stacks = 0;
	CountedCasts.Reset();
	Super::Stop();
}

bool UVeyraGatheringLightPassive::IsPrimed() const
{
	const FVeyraGatheringLightTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindGatheringLight(PassiveId);
	return Tuning && Stacks >= Tuning->StacksToPrime;
}

void UVeyraGatheringLightPassive::OnAbilityHit(const FVeyraAbilityHit& Hit)
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	AActor* Target = Hit.Target.Get();
	const FVeyraGatheringLightTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindGatheringLight(PassiveId);
	// Only her damaging casts on enemy Vanguards count; the fragment itself is not a cast.
	if (!Owner || !Target || !Tuning || Hit.Caster.Get() != Owner || !Hit.bDamaging || Hit.Ability == PassiveId)
	{
		return;
	}
	// Her projectiles and areas can land after she dies; the stacks her death cleared stay cleared until she lives again.
	const AActor* Avatar = Owner->GetAvatarActor();
	if (!Avatar || !VeyraTargeting::IsAlive(Avatar))
	{
		return;
	}
	const AActor* Side = Owner->GetOwner();
	if (!VeyraUnits::IsVanguard(Target) || !VeyraTargeting::AreHostile(Side, Target) || CountedCasts.Contains(Hit.CastId))
	{
		return;
	}
	if (Stacks < Tuning->StacksToPrime)
	{
		++Stacks;
		CountedCasts.Add(Hit.CastId);
		return;
	}
	// Primed: a Vanguard she cannot acquire leaves it primed, and another struck by the same cast may still take the fragment.
	if (!VeyraTargeting::CanAcquire(Side, *Target))
	{
		return;
	}
	CountedCasts.Add(Hit.CastId);
	Stacks = 0;
	LaunchFragment(*Owner, *Target, Hit.CastId);
}

void UVeyraGatheringLightPassive::OnDeath(const FVeyraDeathEvent& Death)
{
	if (Death.Victim.Get() == OwnerAbilitySystem.Get())
	{
		Stacks = 0;
	}
}

void UVeyraGatheringLightPassive::LaunchFragment(UAbilitySystemComponent& Owner, AActor& Target, int32 CastId)
{
	const FVeyraGatheringLightTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindGatheringLight(PassiveId);
	const AActor* Body = Owner.GetAvatarActor();
	UWorld* World = GetWorld();
	if (!Tuning || !Body || !World)
	{
		return;
	}
	AVeyraProjectile* Fragment = World->SpawnActor<AVeyraProjectile>(AVeyraProjectile::StaticClass(), FTransform(Body->GetActorLocation()));
	if (!Fragment)
	{
		UE_LOG(LogVeyraVanguards, Warning, TEXT("Gathering Light could not launch its fragment at %s."), *GetNameSafe(&Target));
		return;
	}
	FVeyraEffectBundleTuning Effects;
	Effects.Damage.Add(Tuning->FragmentDamage);
	// It carries the consuming cast's ID, so its own hit never counts as another cast.
	Fragment->LaunchHoming(Owner, Target, Tuning->Fragment.Speed, Tuning->Fragment.Radius, VeyraEffectDelivery::Prepare(Owner, Effects, GatheringLightRank),
		PassiveId, CastId);
}
