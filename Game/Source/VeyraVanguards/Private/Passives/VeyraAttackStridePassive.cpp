// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Passives/VeyraAttackStridePassive.h"

#include "AbilitySystemComponent.h"
#include "Attacks/VeyraBasicAttackComponent.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraCombatVerbs.h"

void UVeyraAttackStridePassive::Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId)
{
	Super::Start(Owner, InPassiveId);
	const FVeyraAttackStrideTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindAttackStride(PassiveId);
	AActor* Participant = Owner.GetOwner();
	UVeyraBasicAttackComponent* Component = Participant ? Participant->FindComponentByClass<UVeyraBasicAttackComponent>() : nullptr;
	if (!Tuning || !Component)
	{
		return;
	}
	Attacks = Component;
	// She keeps a share of her speed through each windup; a MobileAttack status may raise it (ADR-027 §1).
	Component->SetWindupMovement(Tuning->WindupShare);
	HitHandle = Component->OnHit.AddUObject(this, &UVeyraAttackStridePassive::OnHit);
}

void UVeyraAttackStridePassive::Stop()
{
	if (UVeyraBasicAttackComponent* Component = Attacks.Get())
	{
		Component->SetWindupMovement(0.0);
		Component->OnHit.Remove(HitHandle);
	}
	Attacks.Reset();
	HitHandle.Reset();
	Super::Stop();
}

void UVeyraAttackStridePassive::OnHit(const FVeyraAttackEvent& Event)
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const FVeyraAttackStrideTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindAttackStride(PassiveId);
	// OnHit is her primary attack landing on a unit she could legally attack, so an enemy; only a Vanguard
	// counts (Roster Bible §22).
	if (!Owner || !Tuning || !VeyraUnits::IsVanguard(Event.Target.Get()))
	{
		return;
	}
	const UVeyraProgressionComponent* Progression = Owner->GetOwner()->FindComponentByClass<UVeyraProgressionComponent>();
	const int32 Level = Progression && Progression->IsInitialized() ? Progression->GetLevel() : 1;
	for (const FVeyraContentId& StatusId : Tuning->HitStatuses)
	{
		if (const TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(StatusId, Level))
		{
			VeyraCombat::ApplyStatus(*Owner, *Owner, Status.GetValue());
		}
	}
}
