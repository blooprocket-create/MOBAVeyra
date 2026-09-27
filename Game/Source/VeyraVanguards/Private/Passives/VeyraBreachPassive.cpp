// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Passives/VeyraBreachPassive.h"

#include "AbilitySystemComponent.h"
#include "Attacks/VeyraBasicAttackComponent.h"
#include "Delivery/VeyraEffectDelivery.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"

namespace
{
	// A passive has no ranks; its one amounts apply at every level.
	constexpr int32 BreachRank = 1;
}

void UVeyraBreachPassive::Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId)
{
	Super::Start(Owner, InPassiveId);
	AActor* Participant = Owner.GetOwner();
	if (UVeyraBasicAttackComponent* Component = Participant ? Participant->FindComponentByClass<UVeyraBasicAttackComponent>() : nullptr)
	{
		Attacks = Component;
		ModifyHandle = Component->OnModifyAttack.AddUObject(this, &UVeyraBreachPassive::OnModifyAttack);
	}
}

void UVeyraBreachPassive::Stop()
{
	if (UVeyraBasicAttackComponent* Component = Attacks.Get())
	{
		Component->OnModifyAttack.Remove(ModifyHandle);
	}
	ModifyHandle.Reset();
	Attacks.Reset();
	Super::Stop();
}

void UVeyraBreachPassive::OnModifyAttack(FVeyraAttackPlan& Plan)
{
	const UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const FVeyraBreachTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindBreach(PassiveId);
	// The chain counts only consecutive attacks on one enemy Vanguard, so Breach never ramps past its count.
	if (!Owner || !Tuning || Tuning->HitsToBreach < 1 || Plan.Chain < 1 || Plan.Chain % Tuning->HitsToBreach != 0)
	{
		return;
	}
	Plan.AddDamage(Tuning->BonusDamage.Type, VeyraEffectDelivery::DamageAmount(*Owner, Tuning->BonusDamage, BreachRank));
	Plan.OfferSecondaryImpact(VeyraEffectDelivery::SecondaryImpact(*Owner, Tuning->Impact, BreachRank));
}
