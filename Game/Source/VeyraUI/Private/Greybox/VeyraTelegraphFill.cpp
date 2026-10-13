// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Greybox/VeyraTelegraphFill.h"

#include "Kit/VeyraKitPresentationSettings.h"

FVeyraTelegraphFill VeyraTelegraphFill::Of(const FVeyraPlacedShape& Placed, const FVector& Origin)
{
	FVeyraTelegraphFill Fill;
	const FVector Along = Placed.Direction.GetSafeNormal2D().IsZero() ? FVector::ForwardVector : Placed.Direction.GetSafeNormal2D();
	Fill.Yaw = Along.Rotation().Yaw;
	Fill.Centre = Origin;
	const FVeyraShape& Shape = Placed.Shape;
	switch (Shape.Kind)
	{
	case EVeyraShapeKind::Circle:
		Fill.HalfSize = FVector2D(Shape.Radius);
		break;
	case EVeyraShapeKind::Sector:
		Fill.Shape = 1;
		Fill.HalfSize = FVector2D(Shape.Radius);
		Fill.HalfArc = FMath::DegreesToRadians(Shape.ArcDegrees) / 2.0;
		break;
	case EVeyraShapeKind::Rectangle:
		// From its origin along its direction.
		Fill.Shape = 2;
		Fill.HalfSize = FVector2D(Shape.Length / 2.0, Shape.Width / 2.0);
		Fill.Centre = Origin + Along * (Shape.Length / 2.0);
		break;
	}
	return Fill;
}

EVeyraTelegraphShow VeyraTelegraphFill::ShowOf(const FVeyraAbilityEffects* Effects, EVeyraTelegraphSource Source, bool bSkillOutlines)
{
	if (bSkillOutlines || !Effects)
	{
		return EVeyraTelegraphShow::Full;
	}
	const bool bCast = Source == EVeyraTelegraphSource::Windup || Source == EVeyraTelegraphSource::Channel || Source == EVeyraTelegraphSource::VolleyLane;
	const bool bDelayed = Source == EVeyraTelegraphSource::DelayedArea;
	const bool bLingering = Source == EVeyraTelegraphSource::LingeringArea || Source == EVeyraTelegraphSource::LingeringAreaEnding;
	const bool bLane = Source == EVeyraTelegraphSource::ProjectileLane;
	return Effects->ShowsArea(bCast, bDelayed, bLingering, bLane) ? Effects->Telegraph : EVeyraTelegraphShow::Full;
}

bool VeyraTelegraphFill::IsFilled(EVeyraTelegraphSource Source)
{
	switch (Source)
	{
	case EVeyraTelegraphSource::Windup:
	case EVeyraTelegraphSource::Channel:
	case EVeyraTelegraphSource::DelayedArea:
	case EVeyraTelegraphSource::LingeringArea:
	case EVeyraTelegraphSource::LingeringAreaEnding:
	case EVeyraTelegraphSource::Indicator:
	case EVeyraTelegraphSource::ProjectileLane:
	case EVeyraTelegraphSource::VolleyLane:
		return true;
	case EVeyraTelegraphSource::Selection:
	case EVeyraTelegraphSource::AttackRange:
		break;
	}
	return false;
}

double VeyraTelegraphFill::LandingOf(EVeyraTelegraphSource Source, double RemainingSeconds, double LandingSeconds)
{
	switch (Source)
	{
	case EVeyraTelegraphSource::Windup:
	case EVeyraTelegraphSource::Channel:
	case EVeyraTelegraphSource::DelayedArea:
	case EVeyraTelegraphSource::LingeringAreaEnding:
		return LandingSeconds > 0.0 ? 1.0 - FMath::Clamp(RemainingSeconds / LandingSeconds, 0.0, 1.0) : 1.0;
	case EVeyraTelegraphSource::LingeringArea:
	case EVeyraTelegraphSource::Indicator:
	case EVeyraTelegraphSource::Selection:
	case EVeyraTelegraphSource::ProjectileLane:
	case EVeyraTelegraphSource::AttackRange:
	case EVeyraTelegraphSource::VolleyLane:
		break;
	}
	return 0.0;
}
