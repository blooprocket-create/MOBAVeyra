// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Delivery/VeyraAreaDelivery.h"

#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "Targeting/VeyraTargeting.h"
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
	}
	return Prepared;
}

TArray<AActor*> Resolve(UWorld& World, UAbilitySystemComponent& Caster, const FVeyraEffectFrame& Frame, TConstArrayView<FVeyraPreparedZone> Zones,
	const FVeyraAbilityHitSource& Source)
{
	// Sides belong to the participant, which outlives its body, so a caster who died since Commit still counts.
	const AActor* Side = Caster.GetOwner();
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
			FVeyraAbilityHitSource UnitSource = Source;
			if (Zone.CasterShieldPerVanguard.IsSet() && VeyraUnits::IsVanguard(Unit))
			{
				UnitSource.bCasterShielded = VeyraCombat::GrantShield(Caster, Caster, Zone.CasterShieldPerVanguard.GetValue()).IsValid();
			}
			VeyraEffectDelivery::Apply(Caster, *Unit, Zone.Effects, Frame, UnitSource);
		}
	}
	UE_LOG(LogVeyraAbilities, Verbose, TEXT("An area of %s hit %d unit(s)."), *GetNameSafe(Side), Hit.Num());
	return Hit;
}
}
