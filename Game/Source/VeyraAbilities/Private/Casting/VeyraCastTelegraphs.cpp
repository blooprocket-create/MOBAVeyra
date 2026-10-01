// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Casting/VeyraCastTelegraphs.h"

#include "Casting/VeyraCastStateComponent.h"
#include "Delivery/VeyraAreaDelivery.h"
#include "Tuning/VeyraAbilitiesTuning.h"

namespace
{
	void AddTelegraphZones(TArray<FVeyraPlacedShape>& Shapes, TConstArrayView<FVeyraAreaZoneTuning> Zones, const FVector& Origin, const FVector& Direction)
	{
		for (const FVeyraAreaZoneTuning& Zone : Zones)
		{
			Shapes.Add(FVeyraPlacedShape{ Zone.Shape, Origin, Direction });
		}
	}

	/** A path Length long and Width across, from its origin along its direction. */
	FVeyraShape TelegraphPath(double Length, double Width)
	{
		FVeyraShape Shape;
		Shape.Kind = EVeyraShapeKind::Rectangle;
		Shape.Length = Length;
		Shape.Width = Width;
		return Shape;
	}

	FVeyraShape TelegraphCircle(double Radius)
	{
		FVeyraShape Shape;
		Shape.Kind = EVeyraShapeKind::Circle;
		Shape.Radius = Radius;
		return Shape;
	}
}

TArray<FVeyraPlacedShape> VeyraCastTelegraphs::ForCast(const FVeyraAbilitiesTuning& Tuning, const FVeyraCastState& State, const FVector& CasterLocation,
	double CasterRadius)
{
	TArray<FVeyraPlacedShape> Shapes;
	if (const FVeyraAreaAbilityTuning* Area = Tuning.Area.Find(State.Ability))
	{
		const FVeyraEffectFrame Placement = VeyraAreaDelivery::Place(*Area, CasterLocation, State.Location, State.Direction);
		AddTelegraphZones(Shapes, Area->Zones, Placement.Origin, Placement.Direction);
	}
	else if (const FVeyraSkillshotAbilityTuning* Skillshot = Tuning.Skillshot.Find(State.Ability))
	{
		// The projectile leaves from the caster's body along the cast's direction (UVeyraSkillshotAbility).
		const FVeyraProjectileTuning& Projectile = Skillshot->Projectile;
		Shapes.Add(FVeyraPlacedShape{ TelegraphPath(Projectile.Range, 2.0 * Projectile.Radius), CasterLocation, State.Direction });
	}
	else if (const FVeyraDashAbilityTuning* Dash = Tuning.Dash.Find(State.Ability))
	{
		AddTelegraphZones(Shapes, Dash->StartZones, CasterLocation, State.Direction);
		Shapes.Add(FVeyraPlacedShape{ TelegraphPath(Dash->Distance, 2.0 * CasterRadius), CasterLocation, VeyraAbilityRules::DashHeading(*Dash, State.Direction) });
	}
	else if (const FVeyraSelfBuffAbilityTuning* Buff = Tuning.SelfBuff.Find(State.Ability))
	{
		for (const FVeyraAuraTuning& Aura : Buff->Aura)
		{
			Shapes.Add(FVeyraPlacedShape{ TelegraphCircle(Aura.Radius), CasterLocation, State.Direction });
		}
	}
	return Shapes;
}

TArray<FVeyraPlacedShape> VeyraCastTelegraphs::ForAim(const FVeyraAbilitiesTuning& Tuning, const FVeyraContentId& Ability, const FVector& CasterLocation,
	double CasterRadius, const FVector& AimPoint)
{
	TArray<FVeyraPlacedShape> Shapes;
	// A targeted-damage ability keeps its range apart from the cast tuning the others share.
	const FVeyraTargetedDamageAbilityTuning* Targeted = Tuning.TargetedDamage.Find(Ability);
	const FVeyraCastTuning* Cast = VeyraAbilityRules::FindCast(Tuning, Ability);
	if (!Targeted && !Cast)
	{
		return Shapes;
	}
	const FVector Toward = (AimPoint - CasterLocation).GetSafeNormal2D();
	const FVector Direction = Toward.IsNearlyZero() ? FVector::ForwardVector : Toward;
	const double Range = Targeted ? Targeted->CastRange : Cast->CastRange;
	if (Range > 0.0)
	{
		Shapes.Add(FVeyraPlacedShape{ TelegraphCircle(Range), CasterLocation, Direction });
	}
	FVeyraCastState Aimed;
	Aimed.Ability = Ability;
	Aimed.Location = AimPoint;
	Aimed.Direction = Direction;
	Shapes.Append(ForCast(Tuning, Aimed, CasterLocation, CasterRadius));
	return Shapes;
}
