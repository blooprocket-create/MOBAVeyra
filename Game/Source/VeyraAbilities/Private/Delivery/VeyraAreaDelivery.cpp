// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Delivery/VeyraAreaDelivery.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Engine/World.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraAbilitiesLog.h"

namespace VeyraAreaDelivery
{
namespace
{
	/** The displacement Tuning gives Unit, hit by an area placed at Placement. */
	TOptional<FVeyraDisplacement> DisplacementFor(const FVeyraDisplacementTuning& Tuning, const AActor& Unit, const FVeyraAreaPlacement& Placement,
		double CasterRadius)
	{
		const FVector FromOrigin = (Unit.GetActorLocation() - Placement.Origin).GetSafeNormal2D();
		const FVector Facing = Placement.Direction.GetSafeNormal2D();
		// On the ground, with X forward and Y to the right, the right of (x, y) is (-y, x).
		const FVector Right(-Facing.Y, Facing.X, 0.0);
		FVeyraDisplacement Displacement{ FVector::ZeroVector, Tuning.Distance, Tuning.Speed };
		switch (Tuning.Direction)
		{
		case EVeyraDisplacementDirection::TowardOrigin:
		{
			// A Pull stops at the origin, or at the caster's edge when the caster is the origin.
			const double Gap = FVector::Dist2D(Unit.GetActorLocation(), Placement.Origin) - (Placement.bOriginIsCaster ? Unit.GetSimpleCollisionRadius() + CasterRadius : 0.0);
			Displacement.Direction = -FromOrigin;
			Displacement.Distance = FMath::Min(Tuning.Distance, Gap);
			break;
		}
		case EVeyraDisplacementDirection::AwayFromOrigin:
			Displacement.Direction = FromOrigin.IsNearlyZero() ? Facing : FromOrigin;
			break;
		case EVeyraDisplacementDirection::AcrossCastLeft:
			Displacement.Direction = -Right;
			break;
		case EVeyraDisplacementDirection::AcrossCastRight:
			Displacement.Direction = Right;
			break;
		}
		if (Displacement.Direction.IsNearlyZero() || !(Displacement.Distance > 0.0))
		{
			return {};
		}
		return Displacement;
	}
}

TArray<FVeyraPreparedZone> PrepareZones(UAbilitySystemComponent& Caster, const FVeyraAreaAbilityTuning& Area, int32 Rank)
{
	const double PhysicalPower = Caster.GetNumericAttribute(UVeyraOffenceSet::GetPhysicalPowerAttribute());
	const double MagicPower = Caster.GetNumericAttribute(UVeyraOffenceSet::GetMagicPowerAttribute());
	TArray<FVeyraPreparedZone> Zones;
	for (const FVeyraAreaZoneTuning& ZoneTuning : Area.Zones)
	{
		FVeyraPreparedZone& Zone = Zones.AddDefaulted_GetRef();
		Zone.Shape = ZoneTuning.Shape;
		const FVeyraEffectBundleTuning& Effects = ZoneTuning.Effects;
		if (!Effects.Damage.IsEmpty())
		{
			FVeyraRawDamageEvent Raw;
			for (const FVeyraDamageTuning& Damage : Effects.Damage)
			{
				const double Amount = VeyraAbilityRules::ValueAtRank(Damage.AmountByRank, Rank) + PhysicalPower * Damage.PhysicalPowerRatio
					+ MagicPower * Damage.MagicPowerRatio;
				Raw.Components.Add({ Damage.Type, Amount });
			}
			Zone.Damage = VeyraCombat::PrepareDamage(Caster, Raw);
		}
		for (const FVeyraContentId& StatusId : Effects.Statuses)
		{
			if (const TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(StatusId))
			{
				Zone.Statuses.Add(Status.GetValue());
			}
		}
		if (!Effects.Displacement.IsEmpty())
		{
			Zone.Displacement = Effects.Displacement[0];
		}
	}
	return Zones;
}

TArray<AActor*> Resolve(UWorld& World, UAbilitySystemComponent& Caster, const FVeyraAreaPlacement& Placement, TConstArrayView<FVeyraPreparedZone> Zones)
{
	// Sides belong to the participant, which outlives its body, so a caster who died since Commit still counts.
	const AActor* Side = Caster.GetOwner();
	const AActor* CasterBody = Caster.GetAvatarActor();
	const double CasterRadius = CasterBody ? CasterBody->GetSimpleCollisionRadius() : 0.0;
	TArray<AActor*> Hit;
	for (const FVeyraPreparedZone& Zone : Zones)
	{
		const FVeyraPlacedShape Placed{ Zone.Shape, Placement.Origin, Placement.Direction };
		const TArray<AActor*> Units = VeyraShapes::GatherUnits(World, Placed, [Side, &Hit](const AActor& Unit) {
			return VeyraTargeting::AreHostile(Side, &Unit) && !Hit.ContainsByPredicate([&Unit](const AActor* Earlier) { return Earlier == &Unit; });
		});
		for (AActor* Unit : Units)
		{
			Hit.Add(Unit);
			UAbilitySystemComponent* Target = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Unit);
			if (!Target)
			{
				continue;
			}
			if (Zone.Damage.IsValid())
			{
				VeyraCombat::DealPreparedDamage(Zone.Damage, *Target);
			}
			for (const FVeyraStatusSpec& Status : Zone.Statuses)
			{
				VeyraCombat::ApplyStatus(Caster, *Target, Status);
			}
			if (Zone.Displacement.IsSet())
			{
				if (const TOptional<FVeyraDisplacement> Displacement = DisplacementFor(Zone.Displacement.GetValue(), *Unit, Placement, CasterRadius))
				{
					VeyraCombat::Displace(Caster, *Target, Displacement.GetValue());
				}
			}
		}
	}
	UE_LOG(LogVeyraAbilities, Verbose, TEXT("An area of %s hit %d unit(s)."), *GetNameSafe(Side), Hit.Num());
	return Hit;
}
}
