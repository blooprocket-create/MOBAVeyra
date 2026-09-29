// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Abilities/VeyraEmpoweredAttackAbility.h"

#include "AbilitySystemComponent.h"
#include "Attacks/VeyraBasicAttackComponent.h"
#include "Delivery/VeyraEffectDelivery.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraAbilitiesLog.h"

bool UVeyraEmpoweredAttackAbility::Defines(const FVeyraContentId& Ability) const
{
	return UVeyraAbilitiesTuningSubsystem::FindEmpoweredAttack(Ability) != nullptr;
}

double UVeyraEmpoweredAttackAbility::GetResourceCost(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraEmpoweredAttackAbilityTuning* Empowered = UVeyraAbilitiesTuningSubsystem::FindEmpoweredAttack(Ability);
	return Empowered ? VeyraAbilityRules::ValueAtRank(Empowered->Cast.ResourceCostByRank, Rank) : 0.0;
}

double UVeyraEmpoweredAttackAbility::GetCooldownSeconds(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraEmpoweredAttackAbilityTuning* Empowered = UVeyraAbilitiesTuningSubsystem::FindEmpoweredAttack(Ability);
	return Empowered ? VeyraAbilityRules::ValueAtRank(Empowered->Cast.CooldownSecondsByRank, Rank) : 0.0;
}

const FVeyraCastTuning* UVeyraEmpoweredAttackAbility::GetCastTuning(const FVeyraContentId& Ability) const
{
	const FVeyraEmpoweredAttackAbilityTuning* Empowered = UVeyraAbilitiesTuningSubsystem::FindEmpoweredAttack(Ability);
	return Empowered ? &Empowered->Cast : nullptr;
}

FVeyraChannelPlan UVeyraEmpoweredAttackAbility::Deliver(const FVeyraCast& Cast)
{
	const FVeyraEmpoweredAttackAbilityTuning* Tuning = UVeyraAbilitiesTuningSubsystem::FindEmpoweredAttack(Cast.Ability);
	UAbilitySystemComponent* Caster = Cast.Caster.Get();
	const AActor* Owner = Caster ? Caster->GetOwner() : nullptr;
	UVeyraBasicAttackComponent* Attacks = Owner ? Owner->FindComponentByClass<UVeyraBasicAttackComponent>() : nullptr;
	if (!Tuning || !Attacks)
	{
		UE_LOG(LogVeyraAbilities, Error, TEXT("%s empowered nothing: %s has no basic attacks."), *Cast.Ability.ToString(), *GetNameSafe(Owner));
		return FVeyraChannelPlan();
	}

	FVeyraAttackEmpowerment Empowerment;
	Empowerment.Ability = Cast.Ability;
	Empowerment.CastId = Cast.CastId;
	Empowerment.DurationSeconds = Tuning->DurationSeconds;
	// Applied at the attack's Commit, so the attacker's power is read then (Combat Bible §50).
	Empowerment.Apply = [Empowered = *Tuning, Rank = Cast.Rank, Source = TWeakObjectPtr<UAbilitySystemComponent>(Caster)](FVeyraAttackPlan& Plan) {
		const UAbilitySystemComponent* Attacker = Source.Get();
		if (!Attacker)
		{
			return;
		}
		for (const FVeyraDamageTuning& Damage : Empowered.Damage)
		{
			Plan.AddDamage(Damage.Type, VeyraEffectDelivery::DamageAmount(*Attacker, Damage, Rank));
		}
		Plan.Damage.PhysicalPenetration.Retained *= 1.0 - VeyraAbilityRules::ValueAtRank(Empowered.ArmorPenetrationByRank, Rank);
		Plan.TargetStatuses.Append(VeyraEffectDelivery::StatusSpecs(Empowered.Statuses));
		if (!Empowered.Cleave.IsEmpty())
		{
			Plan.Cleave = FVeyraAttackCleave{ Empowered.Cleave[0].DamageFraction, VeyraEffectDelivery::StatusSpecs(Empowered.Cleave[0].Statuses) };
		}
		if (!Empowered.SecondaryImpact.IsEmpty())
		{
			Plan.OfferSecondaryImpact(VeyraEffectDelivery::SecondaryImpact(*Attacker, Empowered.SecondaryImpact[0], Rank));
		}
	};
	Attacks->Empower(MoveTemp(Empowerment));
	return FVeyraChannelPlan();
}

bool UVeyraEmpoweredAttackAbility::IsOffensive(const FVeyraContentId& /*Ability*/) const
{
	// Empowering is not attacking: the attack it empowers is what ends stealth.
	return false;
}
