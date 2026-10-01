// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Delivery/VeyraAreaDelivery.h"

#include "AbilitySystemComponent.h"
#include "Delivery/VeyraLingeringArea.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Targeting/VeyraTargeting.h"
#include "Targeting/VeyraVisibility.h"
#include "Teams/VeyraTeam.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraAbilitiesLog.h"

namespace VeyraAreaDelivery
{
FVeyraEffectFrame Place(const FVeyraAreaAbilityTuning& Area, const FVector& CasterLocation, const FVector& Point, const FVector& Direction)
{
	FVeyraEffectFrame Placement;
	Placement.bOriginIsCaster = Area.Origin == EVeyraAreaOrigin::Caster;
	Placement.Origin = Placement.bOriginIsCaster ? CasterLocation : Point;
	Placement.Direction = Direction;
	return Placement;
}

AVeyraLingeringArea* FindCastersLingeringArea(const UWorld& World, const UAbilitySystemComponent& Caster, const FVeyraContentId& Ability)
{
	for (TActorIterator<AVeyraLingeringArea> It(&World); It; ++It)
	{
		if (It->GetCaster() == &Caster && It->GetAbility() == Ability && !It->IsActorBeingDestroyed())
		{
			return *It;
		}
	}
	return nullptr;
}

double DelayAt(const UWorld& World, const UAbilitySystemComponent& Caster, const FVeyraAreaAbilityTuning& Area, const FVector& Point)
{
	for (const FVeyraAreaDelayWithinTuning& Within : Area.DelayWithin)
	{
		for (TActorIterator<AVeyraLingeringArea> It(&World); It; ++It)
		{
			if (It->GetCaster() == &Caster && It->GetAbility() == Within.Ability && VeyraShapes::Touches(It->GetPlacedShape(), Point, 0.0))
			{
				return Within.DelaySeconds;
			}
		}
	}
	return Area.DelaySeconds;
}

TArray<FVeyraPreparedZone> PrepareZones(UAbilitySystemComponent& Caster, TConstArrayView<FVeyraAreaZoneTuning> Zones, int32 Rank)
{
	TArray<FVeyraPreparedZone> Prepared;
	for (const FVeyraAreaZoneTuning& Zone : Zones)
	{
		FVeyraPreparedZone& Ready = Prepared.Add_GetRef(FVeyraPreparedZone{ Zone.Shape, VeyraEffectDelivery::Prepare(Caster, Zone.Effects, Rank) });
		if (!Zone.CasterShieldPerVanguard.IsEmpty())
		{
			Ready.CasterShieldPerVanguard = VeyraEffectDelivery::ShieldGrant(Caster, Zone.CasterShieldPerVanguard[0], Rank);
		}
		for (const FVeyraContentId& StatusId : Zone.CasterStatusesPerVanguard)
		{
			if (const TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(StatusId))
			{
				Ready.CasterStatusesPerVanguard.Add(Status.GetValue());
			}
		}
	}
	return Prepared;
}

TArray<AActor*> Resolve(UWorld& World, UAbilitySystemComponent& Caster, const FVeyraEffectFrame& Frame, TConstArrayView<FVeyraPreparedZone> Zones,
	const FVeyraAbilityHitSource& Source)
{
	// Sides belong to the participant, which outlives its body, so a caster who died since Commit still counts.
	const AActor* Side = Caster.GetOwner();
	struct FZoneHit
	{
		AActor* Unit = nullptr;
		const FVeyraPreparedZone* Zone = nullptr;
		FVeyraAbilityHitSource Source;
	};
	TArray<FZoneHit> ZoneHits;
	TArray<AActor*> Hit;
	for (const FVeyraPreparedZone& Zone : Zones)
	{
		const FVeyraPlacedShape Placed{ Zone.Shape, Frame.Origin, Frame.Direction };
		const TArray<AActor*> Units = VeyraShapes::GatherUnits(World, Placed, [Side, &Hit](const AActor& Unit) {
			return VeyraTargeting::AreHostile(Side, &Unit) && !Hit.ContainsByPredicate([&Unit](const AActor* Earlier) { return Earlier == &Unit; });
		});
		for (AActor* Unit : Units)
		{
			Hit.Add(Unit);
			ZoneHits.Add(FZoneHit{ Unit, &Zone, Source });
		}
	}

	// What the caster gains per Vanguard caught comes first: a takedown by the damage that follows then
	// extends statuses that no later refresh from the same cast replaces.
	for (FZoneHit& ZoneHit : ZoneHits)
	{
		if (!VeyraUnits::IsVanguard(ZoneHit.Unit))
		{
			continue;
		}
		if (ZoneHit.Zone->CasterShieldPerVanguard.IsSet())
		{
			ZoneHit.Source.bCasterShielded = VeyraCombat::GrantShield(Caster, Caster, ZoneHit.Zone->CasterShieldPerVanguard.GetValue()).IsValid();
		}
		for (const FVeyraStatusSpec& Status : ZoneHit.Zone->CasterStatusesPerVanguard)
		{
			VeyraCombat::ApplyStatus(Caster, Caster, Status);
		}
	}
	for (const FZoneHit& ZoneHit : ZoneHits)
	{
		VeyraEffectDelivery::Apply(Caster, *ZoneHit.Unit, ZoneHit.Zone->Effects, Frame, ZoneHit.Source);
	}
	UE_LOG(LogVeyraAbilities, Verbose, TEXT("An area of %s hit %d unit(s)."), *GetNameSafe(Side), Hit.Num());
	return Hit;
}

TOptional<FVeyraPreparedLinger> PrepareLinger(UAbilitySystemComponent& Caster, const FVeyraAreaAbilityTuning& Area, int32 Rank, int32 Level,
	const FVeyraContentId& Ability, int32 CastId)
{
	if (Area.Linger.IsEmpty() || Area.Zones.IsEmpty())
	{
		return {};
	}
	const FVeyraLingerTuning& Tuning = Area.Linger[0];
	FVeyraPreparedLinger Linger;
	// It lasts in its outermost zone's shape.
	Linger.Shape = Area.Zones.Last().Shape;
	Linger.DurationSeconds = Tuning.DurationSeconds;
	Linger.PulseSeconds = Tuning.PulseSeconds;
	Linger.Sight = Tuning.Sight;
	Linger.Ability = Ability;
	// Its statuses from the caster's Level at Commit (Combat Bible §50).
	const auto PrepareStatuses = [Level](TConstArrayView<FVeyraContentId> Ids, TArray<FVeyraStatusSpec>& Out) {
		for (const FVeyraContentId& Id : Ids)
		{
			if (const TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(Id, Level))
			{
				Out.Add(Status.GetValue());
			}
		}
	};
	PrepareStatuses(Tuning.CasterStatuses, Linger.Statuses.Caster);
	PrepareStatuses(Tuning.AllyStatuses, Linger.Statuses.Allies);
	PrepareStatuses(Tuning.EnemyStatuses, Linger.Statuses.Enemies);
	// What its pulses and its end do to the enemies inside, from the caster's power at Commit (ADR-026 §4).
	Linger.Effects.EndWarningSeconds = Tuning.EndWarningSeconds;
	Linger.Effects.CastId = CastId;
	const auto PrepareBundles = [&Caster, &Linger, Rank](TConstArrayView<FVeyraEffectBundleTuning> Bundles, TArray<FVeyraPreparedZone>& Out) {
		for (const FVeyraEffectBundleTuning& Bundle : Bundles)
		{
			Out.Add(FVeyraPreparedZone{ Linger.Shape, VeyraEffectDelivery::Prepare(Caster, Bundle, Rank) });
		}
	};
	PrepareBundles(Tuning.PulseEffects, Linger.Effects.Pulse);
	PrepareBundles(Tuning.EndEffects, Linger.Effects.End);
	return Linger;
}

void ArmLinger(UWorld& World, UAbilitySystemComponent& Caster, const FVeyraEffectFrame& Placement, const FVeyraPreparedLinger& Linger)
{
	if (AVeyraLingeringArea* Lingering = World.SpawnActor<AVeyraLingeringArea>(AVeyraLingeringArea::StaticClass(), FTransform(Placement.Origin)))
	{
		Lingering->Arm(Caster, Placement, Linger.Shape, Linger.Statuses, Linger.Effects, Linger.DurationSeconds, Linger.PulseSeconds, Linger.Ability);
	}
	if (Linger.Sight == EVeyraLingerSight::Ordinary)
	{
		VeyraVisibility::RevealShape(World, VeyraTeams::TeamOf(Caster.GetOwner()), FVeyraPlacedShape{ Linger.Shape, Placement.Origin, Placement.Direction },
			Linger.DurationSeconds);
	}
}
}
