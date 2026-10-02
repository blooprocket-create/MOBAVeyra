// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Abilities/VeyraEchoAbility.h"

#include "AbilitySystemComponent.h"
#include "Echoes/VeyraEchoSubsystem.h"
#include "Engine/World.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

bool UVeyraEchoAbility::Defines(const FVeyraContentId& Ability) const
{
	return UVeyraAbilitiesTuningSubsystem::FindEcho(Ability) != nullptr;
}

double UVeyraEchoAbility::GetResourceCost(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraEchoAbilityTuning* Echo = UVeyraAbilitiesTuningSubsystem::FindEcho(Ability);
	return Echo ? VeyraAbilityRules::ValueAtRank(Echo->Cast.ResourceCostByRank, Rank) : 0.0;
}

double UVeyraEchoAbility::GetCooldownSeconds(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraEchoAbilityTuning* Echo = UVeyraAbilitiesTuningSubsystem::FindEcho(Ability);
	return Echo ? VeyraAbilityRules::ValueAtRank(Echo->Cast.CooldownSecondsByRank, Rank) : 0.0;
}

EVeyraCastRejection UVeyraEchoAbility::CheckTarget(const AActor& /*Caster*/, const FVeyraContentId& Ability, const FVeyraCastTarget& Target) const
{
	if (!UVeyraAbilitiesTuningSubsystem::FindEcho(Ability))
	{
		return EVeyraCastRejection::UnknownAbility;
	}
	// It forms at a point, which a point beyond its range is brought back within (ADR-008 §9).
	return HasUsablePoint(Target) ? EVeyraCastRejection::None : EVeyraCastRejection::InvalidLocation;
}

const FVeyraCastTuning* UVeyraEchoAbility::GetCastTuning(const FVeyraContentId& Ability) const
{
	const FVeyraEchoAbilityTuning* Echo = UVeyraAbilitiesTuningSubsystem::FindEcho(Ability);
	return Echo ? &Echo->Cast : nullptr;
}

FVeyraChannelPlan UVeyraEchoAbility::Deliver(const FVeyraCast& Cast)
{
	const FVeyraEchoAbilityTuning* Echo = UVeyraAbilitiesTuningSubsystem::FindEcho(Cast.Ability);
	UAbilitySystemComponent* Caster = Cast.Caster.Get();
	UVeyraEchoSubsystem* Echoes = GetWorld() ? GetWorld()->GetSubsystem<UVeyraEchoSubsystem>() : nullptr;
	if (Echo && Caster && Echoes && !Echo->Manifest.IsEmpty())
	{
		Echoes->Manifest(*Caster, Cast.Ability, Cast.Point);
	}
	else if (Echo && Caster && Echoes && !Echo->Projection.IsEmpty())
	{
		Echoes->Project(*Caster, Cast.Ability, Cast.Point);
	}
	return FVeyraChannelPlan();
}

bool UVeyraEchoAbility::IsOffensive(const FVeyraContentId& /*Ability*/) const
{
	// Forming an Echo threatens no one; what it repeats answers for itself.
	return false;
}
