// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Delivery/VeyraEffectDelivery.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

namespace VeyraEffectDelivery
{
namespace
{
	/** The displacement Tuning gives Unit, measured from Frame. */
	TOptional<FVeyraDisplacement> DisplacementFor(const FVeyraDisplacementTuning& Tuning, const AActor& Unit, const FVeyraEffectFrame& Frame, double CasterRadius)
	{
		const FVector FromOrigin = (Unit.GetActorLocation() - Frame.Origin).GetSafeNormal2D();
		const FVector Facing = Frame.Direction.GetSafeNormal2D();
		// On the ground, with X forward and Y to the right, the right of (x, y) is (-y, x).
		const FVector Right(-Facing.Y, Facing.X, 0.0);
		FVeyraDisplacement Displacement{ FVector::ZeroVector, Tuning.Distance, Tuning.Speed };
		switch (Tuning.Direction)
		{
		case EVeyraDisplacementDirection::TowardOrigin:
		{
			// A Pull stops at the origin, or at the caster's edge when the caster is the origin.
			const double Gap = FVector::Dist2D(Unit.GetActorLocation(), Frame.Origin) - (Frame.bOriginIsCaster ? Unit.GetSimpleCollisionRadius() + CasterRadius : 0.0);
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
		case EVeyraDisplacementDirection::AsideFromPath:
		{
			// To whichever side of the path's line the unit is on; straight ahead of it goes right.
			const double Side = FVector::DotProduct(Unit.GetActorLocation() - Frame.Origin, Right);
			Displacement.Direction = Side < 0.0 ? -Right : Right;
			break;
		}
		}
		if (Displacement.Direction.IsNearlyZero() || !(Displacement.Distance > 0.0))
		{
			return {};
		}
		return Displacement;
	}
}

double DamageAmount(const UAbilitySystemComponent& Caster, const FVeyraDamageTuning& Damage, int32 Rank)
{
	return VeyraAbilityRules::ValueAtRank(Damage.AmountByRank, Rank)
		+ Caster.GetNumericAttribute(UVeyraOffenceSet::GetPhysicalPowerAttribute()) * Damage.PhysicalPowerRatio
		+ Caster.GetNumericAttribute(UVeyraOffenceSet::GetMagicPowerAttribute()) * Damage.MagicPowerRatio;
}

TArray<FVeyraStatusSpec> StatusSpecs(TConstArrayView<FVeyraContentId> Ids)
{
	TArray<FVeyraStatusSpec> Specs;
	for (const FVeyraContentId& StatusId : Ids)
	{
		if (const TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(StatusId))
		{
			Specs.Add(Status.GetValue());
		}
	}
	return Specs;
}

FVeyraPreparedEffects Prepare(UAbilitySystemComponent& Caster, const FVeyraEffectBundleTuning& Effects, int32 Rank)
{
	FVeyraPreparedEffects Prepared;
	if (!Effects.Damage.IsEmpty())
	{
		FVeyraRawDamageEvent Raw;
		for (const FVeyraDamageTuning& Damage : Effects.Damage)
		{
			Raw.Components.Add({ Damage.Type, DamageAmount(Caster, Damage, Rank) });
		}
		Prepared.Damage = VeyraCombat::PrepareDamage(Caster, Raw);
	}
	Prepared.Statuses = StatusSpecs(Effects.Statuses);
	if (!Effects.Displacement.IsEmpty())
	{
		Prepared.Displacement = Effects.Displacement[0];
	}
	return Prepared;
}

bool IsEmpty(const FVeyraPreparedEffects& Effects)
{
	return !Effects.Damage.IsValid() && Effects.Statuses.IsEmpty() && !Effects.Displacement.IsSet();
}

void Apply(UAbilitySystemComponent& Caster, AActor& Unit, const FVeyraPreparedEffects& Effects, const FVeyraEffectFrame& Frame)
{
	UAbilitySystemComponent* Target = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit);
	if (!Target)
	{
		return;
	}
	if (Effects.Damage.IsValid())
	{
		VeyraCombat::DealPreparedDamage(Effects.Damage, *Target);
	}
	for (const FVeyraStatusSpec& Status : Effects.Statuses)
	{
		VeyraCombat::ApplyStatus(Caster, *Target, Status);
	}
	if (Effects.Displacement.IsSet())
	{
		const AActor* CasterBody = Caster.GetAvatarActor();
		const double CasterRadius = CasterBody ? CasterBody->GetSimpleCollisionRadius() : 0.0;
		if (const TOptional<FVeyraDisplacement> Displacement = DisplacementFor(Effects.Displacement.GetValue(), Unit, Frame, CasterRadius))
		{
			VeyraCombat::Displace(Caster, *Target, Displacement.GetValue());
		}
	}
}
}
