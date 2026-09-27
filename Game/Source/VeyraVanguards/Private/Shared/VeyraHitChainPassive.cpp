// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shared/VeyraHitChainPassive.h"

#include "AbilitySystemComponent.h"
#include "Attacks/VeyraBasicAttackComponent.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraCombatVerbs.h"

void UVeyraHitChainPassive::Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId)
{
	Super::Start(Owner, InPassiveId);
	AActor* Participant = Owner.GetOwner();
	UVeyraBasicAttackComponent* Component = Participant ? Participant->FindComponentByClass<UVeyraBasicAttackComponent>() : nullptr;
	if (Component)
	{
		Attacks = Component;
		HitHandle = Component->OnHit.AddUObject(this, &UVeyraHitChainPassive::OnHit);
		ChainResetHandle = Component->OnChainReset.AddUObject(this, &UVeyraHitChainPassive::OnChainReset);
	}
}

void UVeyraHitChainPassive::Stop()
{
	if (UVeyraBasicAttackComponent* Component = Attacks.Get())
	{
		Component->OnHit.Remove(HitHandle);
		Component->OnChainReset.Remove(ChainResetHandle);
	}
	HitHandle.Reset();
	ChainResetHandle.Reset();
	Attacks.Reset();
	Super::Stop();
}

void UVeyraHitChainPassive::OnHit(const FVeyraAttackEvent& Event)
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const FVeyraHitChainTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindHitChain(PassiveId);
	// Only an attack in a chain on an enemy Vanguard counts; any other hit ended the chain already.
	if (!Owner || !Tuning || Event.Chain < 1 || Event.Attacker.Get() != Owner)
	{
		return;
	}
	if (const TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(Tuning->Status))
	{
		VeyraCombat::ApplyStatus(*Owner, *Owner, Status.GetValue());
	}
}

void UVeyraHitChainPassive::OnChainReset()
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const FVeyraHitChainTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindHitChain(PassiveId);
	if (Owner && Tuning)
	{
		VeyraCombat::RemoveStatus(*Owner, Tuning->Status);
	}
}
