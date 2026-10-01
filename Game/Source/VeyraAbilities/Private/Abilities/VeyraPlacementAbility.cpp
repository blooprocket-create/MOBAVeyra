// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Abilities/VeyraPlacementAbility.h"

#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "Entities/VeyraPlacedMarker.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraAbilitiesLog.h"
#include "VeyraCombatVerbs.h"

bool UVeyraPlacementAbility::Defines(const FVeyraContentId& Ability) const
{
	return UVeyraAbilitiesTuningSubsystem::FindPlacement(Ability) != nullptr;
}

double UVeyraPlacementAbility::GetResourceCost(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraPlacementAbilityTuning* Placement = UVeyraAbilitiesTuningSubsystem::FindPlacement(Ability);
	return Placement ? VeyraAbilityRules::ValueAtRank(Placement->Cast.ResourceCostByRank, Rank) : 0.0;
}

double UVeyraPlacementAbility::GetCooldownSeconds(const FVeyraContentId& Ability, int32 Rank) const
{
	const FVeyraPlacementAbilityTuning* Placement = UVeyraAbilitiesTuningSubsystem::FindPlacement(Ability);
	return Placement ? VeyraAbilityRules::ValueAtRank(Placement->Cast.CooldownSecondsByRank, Rank) : 0.0;
}

EVeyraCastRejection UVeyraPlacementAbility::CheckTarget(const AActor& /*Caster*/, const FVeyraContentId& /*Ability*/, const FVeyraCastTarget& Target) const
{
	// The point says where; the cast brings it within range.
	return HasUsablePoint(Target) ? EVeyraCastRejection::None : EVeyraCastRejection::InvalidLocation;
}

const FVeyraCastTuning* UVeyraPlacementAbility::GetCastTuning(const FVeyraContentId& Ability) const
{
	const FVeyraPlacementAbilityTuning* Placement = UVeyraAbilitiesTuningSubsystem::FindPlacement(Ability);
	return Placement ? &Placement->Cast : nullptr;
}

FVeyraChannelPlan UVeyraPlacementAbility::Deliver(const FVeyraCast& Cast)
{
	const FVeyraPlacementAbilityTuning* Placement = UVeyraAbilitiesTuningSubsystem::FindPlacement(Cast.Ability);
	UAbilitySystemComponent* Caster = Cast.Caster.Get();
	const AActor* Body = Caster ? Caster->GetAvatarActor() : nullptr;
	UWorld* World = GetWorld();
	if (!Placement || !Caster || !Body || !World)
	{
		return FVeyraChannelPlan();
	}
	// One at a time: the last goes quietly, and the follow-up this cast opened stays with the new one.
	if (AVeyraPlacedMarker* Last = AVeyraPlacedMarker::FindStanding(*Caster, Cast.Ability))
	{
		Last->EndMarker(EVeyraMarkerEndReason::Replaced);
	}
	// On the ground nearest its point, standing as its caster stands (ADR-031 §4).
	const FVector Ground = VeyraCombat::NearestGround(*World, Cast.Point);
	const FTransform Where(Body->GetActorRotation(), FVector(Ground.X, Ground.Y, Body->GetActorLocation().Z));
	FVeyraMarkerSpec Spec;
	Spec.Id = Cast.Ability;
	Spec.LifetimeSeconds = Placement->Marker.LifetimeSeconds;
	Spec.HitsToDestroy = Placement->Marker.HitsToDestroy;
	Spec.bPresentsAsOwner = Placement->Marker.Look == EVeyraMarkerLook::AsOwner;
	if (!AVeyraPlacedMarker::Place(*World, *Caster, Spec, Where))
	{
		UE_LOG(LogVeyraAbilities, Warning, TEXT("%s could not place its %s (cast %d)."), *GetNameSafe(Body), *Cast.Ability.ToString(), Cast.CastId);
		return FVeyraChannelPlan();
	}
	Placer = Caster;
	UVeyraCombatEventSubsystem* Events = World->GetSubsystem<UVeyraCombatEventSubsystem>();
	if (Events && !MarkerEndHandle.IsValid())
	{
		MarkerEndHandle = Events->OnMarkerEnded.AddUObject(this, &UVeyraPlacementAbility::OnMarkerEnded);
	}
	return FVeyraChannelPlan();
}

void UVeyraPlacementAbility::OnMarkerEnded(const FVeyraMarkerEnd& End)
{
	UAbilitySystemComponent* Caster = Placer.Get();
	if (Caster && End.Owner.Get() == Caster && Defines(End.Id) && End.Reason != EVeyraMarkerEndReason::Replaced)
	{
		// Whatever it offered, as a swap, has nothing left to offer (ADR-031 §4).
		EndRecastWindow(*Caster, End.Id);
	}
}

bool UVeyraPlacementAbility::IsOffensive(const FVeyraContentId& /*Ability*/) const
{
	// Placing a marker threatens no one, so it keeps its caster hidden (ADR-030 §1).
	return false;
}
