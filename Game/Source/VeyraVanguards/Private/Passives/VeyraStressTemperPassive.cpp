// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Passives/VeyraStressTemperPassive.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Engine/World.h"
#include "Events/VeyraAbilityEvents.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraCombatVerbs.h"
#include "VeyraVanguardsLog.h"

namespace
{
	void Give(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target, const FVeyraContentId& StatusId)
	{
		if (const TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(StatusId))
		{
			VeyraCombat::ApplyStatus(Source, Target, Status.GetValue());
		}
	}
}

void UVeyraStressTemperPassive::Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId)
{
	Super::Start(Owner, InPassiveId);
	UWorld* World = GetWorld();
	if (UVeyraAbilityEventSubsystem* Events = World ? World->GetSubsystem<UVeyraAbilityEventSubsystem>() : nullptr)
	{
		HitHandle = Events->OnAbilityHit.AddUObject(this, &UVeyraStressTemperPassive::OnAbilityHit);
	}
	if (UVeyraCombatEventSubsystem* Events = World ? World->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		MovedHandle = Events->OnUnitMoved.AddUObject(this, &UVeyraStressTemperPassive::OnUnitMoved);
	}
}

void UVeyraStressTemperPassive::Stop()
{
	UWorld* World = GetWorld();
	if (UVeyraAbilityEventSubsystem* Events = World ? World->GetSubsystem<UVeyraAbilityEventSubsystem>() : nullptr)
	{
		Events->OnAbilityHit.Remove(HitHandle);
	}
	if (UVeyraCombatEventSubsystem* Events = World ? World->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		Events->OnUnitMoved.Remove(MovedHandle);
	}
	HitHandle.Reset();
	MovedHandle.Reset();
	Super::Stop();
}

void UVeyraStressTemperPassive::OnAbilityHit(const FVeyraAbilityHit& Hit)
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	AActor* Target = Hit.Target.Get();
	const FVeyraStressTemperTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindStressTemper(PassiveId);
	if (!Owner || !Target || !Tuning || Hit.Caster.Get() != Owner || !Hit.bDamaging)
	{
		return;
	}
	// The coating is a Vanguard term, put only on living enemies (ADR-032 §2).
	UAbilitySystemComponent* Coated = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (Coated && VeyraUnits::IsVanguard(Target) && VeyraTargeting::AreHostile(Owner->GetOwner(), Target) && VeyraTargeting::IsAlive(Target))
	{
		Give(*Owner, *Coated, Tuning->Coating);
	}
}

void UVeyraStressTemperPassive::OnUnitMoved(const FVeyraUnitMovedEvent& Moved)
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	UAbilitySystemComponent* Unit = Moved.Unit.Get();
	const AActor* Body = Unit ? Unit->GetAvatarActor() : nullptr;
	const FVeyraStressTemperTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindStressTemper(PassiveId);
	if (!Owner || !Body || !Tuning || !VeyraTargeting::IsAlive(Body) || !VeyraCombat::HasStatusFrom(Body, Tuning->Coating, *Owner)
		|| VeyraCombat::HasStatusFrom(Body, Tuning->Lockout, *Owner))
	{
		return;
	}
	// The lockout first, so nothing the strike sets off strikes again.
	VeyraCombat::RemoveStatus(*Unit, Tuning->Coating);
	Give(*Owner, *Unit, Tuning->Lockout);
	for (const FVeyraContentId& Status : Tuning->StrikeStatuses)
	{
		Give(*Owner, *Unit, Status);
	}
	const UVeyraProgressionComponent* Progression = Owner->GetOwner() ? Owner->GetOwner()->FindComponentByClass<UVeyraProgressionComponent>() : nullptr;
	const int32 Level = Progression && Progression->IsInitialized() ? Progression->GetLevel() : 1;
	const double Power = Owner->GetNumericAttribute(UVeyraOffenceSet::GetMagicPowerAttribute());
	const double Amount = Tuning->DamageAmount + Tuning->DamagePerLevel * (Level - 1) + Tuning->MagicPowerRatio * Power;
	if (Amount > 0.0)
	{
		FVeyraRawDamageEvent Event;
		Event.Components.Add({ Tuning->DamageType, Amount });
		VeyraCombat::DealDamage(*Owner, *Unit, Event);
	}
	UE_LOG(LogVeyraVanguards, Verbose, TEXT("%s's %s strikes %s where it landed."), *GetNameSafe(Owner->GetOwner()), *PassiveId.ToString(), *GetNameSafe(Body));
}
