// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Abilities/VeyraAreaAbility.h"

#include "AbilitySystemComponent.h"
#include "Delivery/VeyraDelayedArea.h"
#include "Engine/World.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

bool UVeyraAreaAbility::Defines(const FVeyraContentId& Ability) const
{
	return UVeyraAbilitiesTuningSubsystem::FindArea(Ability) != nullptr;
}

double UVeyraAreaAbility::GetResourceCost(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraAreaAbilityTuning* Area = UVeyraAbilitiesTuningSubsystem::FindArea(Ability);
	return Area ? VeyraAbilityRules::ValueAtRank(Area->Cast.ResourceCostByRank, Rank) : 0.0;
}

double UVeyraAreaAbility::GetCooldownSeconds(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraAreaAbilityTuning* Area = UVeyraAbilitiesTuningSubsystem::FindArea(Ability);
	return Area ? VeyraAbilityRules::ValueAtRank(Area->Cast.CooldownSecondsByRank, Rank) : 0.0;
}

EVeyraCastRejection UVeyraAreaAbility::CheckTarget(const AActor& /*Caster*/, const FVeyraContentId& Ability, const FVeyraCastTarget& Target) const
{
	const FVeyraAreaAbilityTuning* Area = UVeyraAbilitiesTuningSubsystem::FindArea(Ability);
	if (!Area)
	{
		return EVeyraCastRejection::UnknownAbility;
	}
	// An area on the caster may be aimed; one at a ground point needs the point.
	if ((Area->Origin == EVeyraAreaOrigin::TargetPoint || Target.bHasLocation) && !HasUsablePoint(Target))
	{
		return EVeyraCastRejection::InvalidLocation;
	}
	return EVeyraCastRejection::None;
}

const FVeyraCastTuning* UVeyraAreaAbility::GetCastTuning(const FVeyraContentId& Ability) const
{
	const FVeyraAreaAbilityTuning* Area = UVeyraAbilitiesTuningSubsystem::FindArea(Ability);
	return Area ? &Area->Cast : nullptr;
}

FVeyraChannelPlan UVeyraAreaAbility::Deliver(const FVeyraCast& Cast)
{
	const FVeyraAreaAbilityTuning* Area = UVeyraAbilitiesTuningSubsystem::FindArea(Cast.Ability);
	UAbilitySystemComponent* Caster = Cast.Caster.Get();
	UWorld* World = GetWorld();
	if (!Area || !Caster || !World)
	{
		return FVeyraChannelPlan();
	}

	// An area on the caster lands where the caster is at Commit, which a free windup may have moved.
	const AActor* Body = Caster->GetAvatarActor();
	FVeyraEffectFrame Placement;
	Placement.bOriginIsCaster = Area->Origin == EVeyraAreaOrigin::Caster;
	Placement.Origin = Placement.bOriginIsCaster ? (Body ? Body->GetActorLocation() : Cast.CasterLocation) : Cast.Point;
	Placement.Direction = Cast.Direction;
	TArray<FVeyraPreparedZone> Zones = VeyraAreaDelivery::PrepareZones(*Caster, Area->Zones, Cast.Rank);

	if (Area->DelaySeconds > 0.0)
	{
		AVeyraDelayedArea* Delayed = World->SpawnActor<AVeyraDelayedArea>(AVeyraDelayedArea::StaticClass(), FTransform(Placement.Origin));
		if (Delayed)
		{
			Delayed->Arm(*Caster, Placement, MoveTemp(Zones), Area->DelaySeconds, Cast.Ability, Cast.CastId);
		}
		return FVeyraChannelPlan();
	}
	if (Area->ChannelTicks > 1)
	{
		ChannelPlacement = Placement;
		ChannelZones = MoveTemp(Zones);
		return FVeyraChannelPlan{ Area->ChannelTicks, Area->ChannelSeconds };
	}
	VeyraAreaDelivery::Resolve(*World, *Caster, Placement, Zones, FVeyraAbilityHitSource{ Cast.Ability, Cast.CastId });
	return FVeyraChannelPlan();
}

void UVeyraAreaAbility::DeliverChannelTick(const FVeyraCast& Cast, int32 /*Tick*/)
{
	UAbilitySystemComponent* Caster = Cast.Caster.Get();
	if (Caster && GetWorld())
	{
		VeyraAreaDelivery::Resolve(*GetWorld(), *Caster, ChannelPlacement, ChannelZones, FVeyraAbilityHitSource{ Cast.Ability, Cast.CastId });
	}
}
