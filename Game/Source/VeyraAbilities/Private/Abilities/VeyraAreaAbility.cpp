// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Abilities/VeyraAreaAbility.h"

#include "Attributes/VeyraVitalsSet.h"
#include "Units/VeyraUnit.h"
#include "VeyraCombatVerbs.h"
#include "AbilitySystemComponent.h"
#include "Delivery/VeyraDelayedArea.h"
#include "Delivery/VeyraLingeringArea.h"
#include "Engine/World.h"
#include "Targeting/VeyraVisibility.h"
#include "Teams/VeyraTeam.h"
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
	const FVeyraEffectFrame Placement = VeyraAreaDelivery::Place(*Area, Body ? Body->GetActorLocation() : Cast.CasterLocation, Cast.Point, Cast.Direction);
	TArray<FVeyraPreparedZone> Zones = VeyraAreaDelivery::PrepareZones(*Caster, Area->Zones, Cast.Rank);
	// What it spends of its caster's own, as it commits (ADR-018 §6).
	for (const FVeyraContentId& Spent : Area->ConsumesCasterStatuses)
	{
		VeyraCombat::RemoveStatus(*Caster, Spent);
	}
	// What it puts on its caster as it commits, such as the Slow it channels under (ADR-018 §6).
	for (const FVeyraContentId& StatusId : Area->CasterStatuses)
	{
		if (const TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(StatusId, GetCasterLevel(*Caster)))
		{
			VeyraCombat::ApplyStatus(*Caster, *Caster, Status.GetValue());
		}
	}
	HealedThisCast = 0.0;

	// It lights its area for its caster's side as it commits (ADR-016 §5).
	if (Area->Reveal.Radius > 0.0)
	{
		VeyraVisibility::RevealArea(*World, VeyraTeams::TeamOf(Caster->GetOwner()), Placement.Origin, Area->Reveal.Radius, Area->Reveal.DurationSeconds);
	}

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
		FVeyraChannelPlan Plan{ Area->ChannelTicks, Area->ChannelSeconds };
		Plan.bLocksMovement = Area->ChannelMovement == EVeyraCastMovement::Locked;
		return Plan;
	}
	const TArray<AActor*> Hit = VeyraAreaDelivery::Resolve(*World, *Caster, Placement, Zones, FVeyraAbilityHitSource{ Cast.Ability, Cast.CastId });
	HealFromHits(*Caster, *Area, Hit);
	if (!Area->Linger.IsEmpty() && !Area->Zones.IsEmpty())
	{
		Linger(*Caster, Placement, *Area, Cast);
	}
	return FVeyraChannelPlan();
}

void UVeyraAreaAbility::Linger(UAbilitySystemComponent& Caster, const FVeyraEffectFrame& Placement, const FVeyraAreaAbilityTuning& Area, const FVeyraCast& Cast) const
{
	UWorld* World = GetWorld();
	const FVeyraLingerTuning& Tuning = Area.Linger[0];
	// Its statuses from the caster's Level at Commit (Combat Bible §50).
	const int32 Level = GetCasterLevel(Caster);
	FVeyraLingerStatuses Statuses;
	const auto Prepare = [Level](TConstArrayView<FVeyraContentId> Ids, TArray<FVeyraStatusSpec>& Out) {
		for (const FVeyraContentId& Id : Ids)
		{
			if (const TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(Id, Level))
			{
				Out.Add(Status.GetValue());
			}
		}
	};
	Prepare(Tuning.CasterStatuses, Statuses.Caster);
	Prepare(Tuning.AllyStatuses, Statuses.Allies);
	Prepare(Tuning.EnemyStatuses, Statuses.Enemies);
	// It lasts in its outermost zone's shape.
	const FVeyraShape& Shape = Area.Zones.Last().Shape;
	if (AVeyraLingeringArea* Lingering = World->SpawnActor<AVeyraLingeringArea>(AVeyraLingeringArea::StaticClass(), FTransform(Placement.Origin)))
	{
		Lingering->Arm(Caster, Placement, Shape, MoveTemp(Statuses), Tuning.DurationSeconds, Tuning.PulseSeconds, Cast.Ability);
	}
	if (Tuning.Sight == EVeyraLingerSight::Ordinary)
	{
		VeyraVisibility::RevealShape(*World, VeyraTeams::TeamOf(Caster.GetOwner()), FVeyraPlacedShape{ Shape, Placement.Origin, Placement.Direction },
			Tuning.DurationSeconds);
	}
}

void UVeyraAreaAbility::DeliverChannelTick(const FVeyraCast& Cast, int32 /*Tick*/)
{
	UAbilitySystemComponent* Caster = Cast.Caster.Get();
	const FVeyraAreaAbilityTuning* Area = UVeyraAbilitiesTuningSubsystem::FindArea(Cast.Ability);
	if (!Caster || !Area || !GetWorld())
	{
		return;
	}
	// A caster free to move sweeps where it stands now, the way it faces (ADR-018 §6).
	const AActor* Body = Caster->GetAvatarActor();
	if (Body && Area->ChannelMovement == EVeyraCastMovement::Free && Area->Origin == EVeyraAreaOrigin::Caster)
	{
		ChannelPlacement.Origin = Body->GetActorLocation();
		const FVector Facing = Body->GetActorForwardVector().GetSafeNormal2D();
		ChannelPlacement.Direction = Facing.IsNearlyZero() ? ChannelPlacement.Direction : Facing;
	}
	const TArray<AActor*> Hit = VeyraAreaDelivery::Resolve(*GetWorld(), *Caster, ChannelPlacement, ChannelZones, FVeyraAbilityHitSource{ Cast.Ability, Cast.CastId });
	HealFromHits(*Caster, *Area, Hit);
}

void UVeyraAreaAbility::HealFromHits(UAbilitySystemComponent& Caster, const FVeyraAreaAbilityTuning& Area, TConstArrayView<AActor*> Hit)
{
	if (Area.HealOnHit.IsEmpty())
	{
		return;
	}
	const FVeyraHealOnHitTuning& Heal = Area.HealOnHit[0];
	int32 Counted = 0;
	for (const AActor* Unit : Hit)
	{
		const TOptional<EVeyraUnitKind> Kind = VeyraUnits::KindOf(Unit);
		Counted += Heal.UnitKinds.IsEmpty() || (Kind.IsSet() && Heal.UnitKinds.Contains(Kind.GetValue())) ? 1 : 0;
	}
	// A share of Max Health per unit hit, never past the cast's cap (Roster Bible §23: "bounded Health up to a cap per cast").
	const double MaxHealth = Caster.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute());
	const double Room = FMath::Max(0.0, Heal.CapMaxHealthRatio * MaxHealth - HealedThisCast);
	const double Amount = FMath::Min(Counted * Heal.MaxHealthRatioPerHit * MaxHealth, Room);
	if (Amount > 0.0)
	{
		HealedThisCast += Amount;
		VeyraCombat::RestoreHealthFrom(Caster, Caster, Amount);
	}
}
