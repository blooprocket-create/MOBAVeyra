// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Abilities/VeyraTetherAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "Tethers/VeyraTetherSubsystem.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraAbilitiesLog.h"

bool UVeyraTetherAbility::Defines(const FVeyraContentId& Ability) const
{
	return UVeyraAbilitiesTuningSubsystem::FindTether(Ability) != nullptr;
}

double UVeyraTetherAbility::GetResourceCost(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraTetherAbilityTuning* Tether = UVeyraAbilitiesTuningSubsystem::FindTether(Ability);
	return Tether ? VeyraAbilityRules::ValueAtRank(Tether->Cast.ResourceCostByRank, Rank) : 0.0;
}

double UVeyraTetherAbility::GetCooldownSeconds(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraTetherAbilityTuning* Tether = UVeyraAbilitiesTuningSubsystem::FindTether(Ability);
	return Tether ? VeyraAbilityRules::ValueAtRank(Tether->Cast.CooldownSecondsByRank, Rank) : 0.0;
}

EVeyraCastRejection UVeyraTetherAbility::CheckTarget(const AActor& Caster, const FVeyraContentId& Ability, const FVeyraCastTarget& Target) const
{
	const FVeyraTetherAbilityTuning* Tether = UVeyraAbilitiesTuningSubsystem::FindTether(Ability);
	return Tether ? CheckEnemyUnit(Caster, Target.Actor, Tether->Cast.CastRange, Tether->TargetKinds) : EVeyraCastRejection::UnknownAbility;
}

const FVeyraCastTuning* UVeyraTetherAbility::GetCastTuning(const FVeyraContentId& Ability) const
{
	const FVeyraTetherAbilityTuning* Tether = UVeyraAbilitiesTuningSubsystem::FindTether(Ability);
	return Tether ? &Tether->Cast : nullptr;
}

FVeyraChannelPlan UVeyraTetherAbility::Deliver(const FVeyraCast& Cast)
{
	const FVeyraTetherAbilityTuning* Tuning = UVeyraAbilitiesTuningSubsystem::FindTether(Cast.Ability);
	UAbilitySystemComponent* Caster = Cast.Caster.Get();
	UAbilitySystemComponent* Target = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Cast.TargetActor.Get());
	UVeyraTetherSubsystem* Tethers = GetWorld() ? GetWorld()->GetSubsystem<UVeyraTetherSubsystem>() : nullptr;
	if (!Tuning || !Caster || !Target || !Tethers)
	{
		return FVeyraChannelPlan();
	}
	FVeyraTetherSpec Spec;
	Spec.Id = Cast.Ability;
	Spec.MaxRange = Tuning->MaxRange;
	Spec.DurationSeconds = Tuning->DurationSeconds;
	Spec.SnapDistance = Tuning->SnapDistance;
	Spec.SnapSpeed = Tuning->SnapSpeed;
	const int32 Level = GetCasterLevel(*Caster);
	for (const FVeyraContentId& StatusId : Tuning->TargetStatuses)
	{
		if (const TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(StatusId, Level))
		{
			Spec.TargetStatuses.Add(Status.GetValue());
		}
	}
	if (!Tethers->Tether(*Caster, *Target, Spec))
	{
		// Commit is final (Combat Bible §54): a target that died during the windup keeps the cost spent.
		UE_LOG(LogVeyraAbilities, Verbose, TEXT("%s could not tether %s for %s (cast %d)."), *GetNameSafe(Caster->GetAvatarActor()),
			*GetNameSafe(Cast.TargetActor.Get()), *Cast.Ability.ToString(), Cast.CastId);
	}
	return FVeyraChannelPlan();
}
