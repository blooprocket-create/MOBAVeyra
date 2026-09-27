// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Abilities/VeyraSkillshotAbility.h"

#include "AbilitySystemComponent.h"
#include "Delivery/VeyraProjectile.h"
#include "Engine/World.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

bool UVeyraSkillshotAbility::Defines(const FVeyraContentId& Ability) const
{
	return UVeyraAbilitiesTuningSubsystem::FindSkillshot(Ability) != nullptr;
}

double UVeyraSkillshotAbility::GetResourceCost(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraSkillshotAbilityTuning* Skillshot = UVeyraAbilitiesTuningSubsystem::FindSkillshot(Ability);
	return Skillshot ? VeyraAbilityRules::ValueAtRank(Skillshot->Cast.ResourceCostByRank, Rank) : 0.0;
}

double UVeyraSkillshotAbility::GetCooldownSeconds(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraSkillshotAbilityTuning* Skillshot = UVeyraAbilitiesTuningSubsystem::FindSkillshot(Ability);
	return Skillshot ? VeyraAbilityRules::ValueAtRank(Skillshot->Cast.CooldownSecondsByRank, Rank) : 0.0;
}

EVeyraCastRejection UVeyraSkillshotAbility::CheckTarget(const AActor& /*Caster*/, const FVeyraContentId& Ability, const FVeyraCastTarget& Target) const
{
	if (!UVeyraAbilitiesTuningSubsystem::FindSkillshot(Ability))
	{
		return EVeyraCastRejection::UnknownAbility;
	}
	// The point aims it.
	return HasUsablePoint(Target) ? EVeyraCastRejection::None : EVeyraCastRejection::InvalidLocation;
}

const FVeyraCastTuning* UVeyraSkillshotAbility::GetCastTuning(const FVeyraContentId& Ability) const
{
	const FVeyraSkillshotAbilityTuning* Skillshot = UVeyraAbilitiesTuningSubsystem::FindSkillshot(Ability);
	return Skillshot ? &Skillshot->Cast : nullptr;
}

FVeyraChannelPlan UVeyraSkillshotAbility::Deliver(const FVeyraCast& Cast)
{
	const FVeyraSkillshotAbilityTuning* Skillshot = UVeyraAbilitiesTuningSubsystem::FindSkillshot(Cast.Ability);
	UAbilitySystemComponent* Caster = Cast.Caster.Get();
	const AActor* Body = Caster ? Caster->GetAvatarActor() : nullptr;
	UWorld* World = GetWorld();
	if (!Skillshot || !Caster || !Body || !World)
	{
		return FVeyraChannelPlan();
	}

	// It sets off from where the caster is at Commit, which a free windup may have moved.
	const FTransform Launch(Cast.Direction.Rotation(), Body->GetActorLocation());
	if (AVeyraProjectile* Projectile = World->SpawnActor<AVeyraProjectile>(AVeyraProjectile::StaticClass(), Launch))
	{
		Projectile->LaunchLine(*Caster, Cast.Direction, Skillshot->Projectile, Skillshot->Collision,
			VeyraEffectDelivery::Prepare(*Caster, Skillshot->Effects, Cast.Rank), VeyraEffectDelivery::Prepare(*Caster, Skillshot->PassThroughEffects, Cast.Rank),
			Cast.Ability, Cast.CastId);
	}
	return FVeyraChannelPlan();
}
