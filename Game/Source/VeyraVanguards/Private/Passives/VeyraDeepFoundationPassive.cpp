// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Passives/VeyraDeepFoundationPassive.h"

#include "AbilitySystemComponent.h"
#include "Delivery/VeyraEffectDelivery.h"
#include "Engine/World.h"
#include "Events/VeyraAbilityEvents.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraCombatVerbs.h"

namespace
{
	// A passive has no ranks; its shield's one amount applies at every level.
	constexpr int32 PassiveRank = 1;
}

void UVeyraDeepFoundationPassive::Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId)
{
	Super::Start(Owner, InPassiveId);
	if (UVeyraAbilityEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UVeyraAbilityEventSubsystem>() : nullptr)
	{
		HitHandle = Events->OnAbilityHit.AddUObject(this, &UVeyraDeepFoundationPassive::OnAbilityHit);
	}
}

void UVeyraDeepFoundationPassive::Stop()
{
	if (UVeyraAbilityEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UVeyraAbilityEventSubsystem>() : nullptr)
	{
		Events->OnAbilityHit.Remove(HitHandle);
	}
	HitHandle.Reset();
	LastGrantedAt.Reset();
	Super::Stop();
}

void UVeyraDeepFoundationPassive::OnAbilityHit(const FVeyraAbilityHit& Hit)
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	AActor* Target = Hit.Target.Get();
	const FVeyraDeepFoundationTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindDeepFoundation(PassiveId);
	if (!Owner || !Target || !Tuning || Hit.Caster.Get() != Owner)
	{
		return;
	}
	// Only immobilizing an enemy Vanguard counts, and not a hit whose ability paid a shield for it.
	const bool bImmobilized = Hit.bStunned || Hit.bDisplaced;
	if (!bImmobilized || Hit.bCasterShielded || !VeyraUnits::IsVanguard(Target) || !VeyraTargeting::AreHostile(Owner->GetOwner(), Target))
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (const double* Last = LastGrantedAt.Find(Target); Last && Now < *Last + Tuning->LockoutSeconds)
	{
		return;
	}
	LastGrantedAt.Add(Target, Now);
	VeyraCombat::GrantShield(*Owner, *Owner, VeyraEffectDelivery::ShieldGrant(*Owner, Tuning->Shield, PassiveRank));
}
