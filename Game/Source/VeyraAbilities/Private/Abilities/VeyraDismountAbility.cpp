// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Abilities/VeyraDismountAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraAbilitiesLog.h"
#include "VeyraCombatVerbs.h"

bool UVeyraDismountAbility::Defines(const FVeyraContentId& Ability) const
{
	return UVeyraAbilitiesTuningSubsystem::FindDismount(Ability) != nullptr;
}

double UVeyraDismountAbility::GetResourceCost(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraDismountAbilityTuning* Dismount = UVeyraAbilitiesTuningSubsystem::FindDismount(Ability);
	return Dismount ? VeyraAbilityRules::ValueAtRank(Dismount->Cast.ResourceCostByRank, Rank) : 0.0;
}

double UVeyraDismountAbility::GetCooldownSeconds(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraDismountAbilityTuning* Dismount = UVeyraAbilitiesTuningSubsystem::FindDismount(Ability);
	return Dismount ? VeyraAbilityRules::ValueAtRank(Dismount->Cast.CooldownSecondsByRank, Rank) : 0.0;
}

EVeyraCastRejection UVeyraDismountAbility::CheckTarget(const AActor& Caster, const FVeyraContentId& Ability, const FVeyraCastTarget& /*Target*/) const
{
	const UAbilitySystemComponent* Abilities = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Caster);
	if (!UVeyraAbilitiesTuningSubsystem::FindDismount(Ability) || !Abilities)
	{
		return EVeyraCastRejection::UnknownAbility;
	}
	// Off its ride there is nothing to leave.
	return VeyraCombat::IsRiding(*Abilities) ? EVeyraCastRejection::None : EVeyraCastRejection::InvalidTarget;
}

const FVeyraCastTuning* UVeyraDismountAbility::GetCastTuning(const FVeyraContentId& Ability) const
{
	const FVeyraDismountAbilityTuning* Dismount = UVeyraAbilitiesTuningSubsystem::FindDismount(Ability);
	return Dismount ? &Dismount->Cast : nullptr;
}

FVeyraChannelPlan UVeyraDismountAbility::Deliver(const FVeyraCast& Cast)
{
	UAbilitySystemComponent* Caster = Cast.Caster.Get();
	if (Caster && VeyraCombat::IsRiding(*Caster))
	{
		// Its ride ends as if its rider left it; the ride's own end crashes it (ADR-035 §3).
		VeyraCombat::EndRide(*Caster, EVeyraRideEndReason::Dismounted);
		UE_LOG(LogVeyraAbilities, Verbose, TEXT("%s dismounts with %s (cast %d)."), *GetNameSafe(Caster->GetOwner()), *Cast.Ability.ToString(), Cast.CastId);
	}
	return FVeyraChannelPlan();
}

bool UVeyraDismountAbility::IsOffensive(const FVeyraContentId& /*Ability*/) const
{
	// Leaving a ride threatens no one; what its end does answers for itself.
	return false;
}
