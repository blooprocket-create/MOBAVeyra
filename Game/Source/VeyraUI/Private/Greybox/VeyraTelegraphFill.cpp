// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Greybox/VeyraTelegraphFill.h"

#include "Kit/VeyraKitPresentationSettings.h"

namespace
{
	// How many points along a rim or an edge, and across a rectangle, a footprint is sampled at: enough to find a ridge or
	// a hollow under an area at the match camera's scale (an algorithmic resolution, not a balance value).
	constexpr int32 FootprintRimPoints = 8;
	constexpr int32 FootprintAcross = 3;
}

TArray<FVector2D> VeyraTelegraphFill::FootprintOf(const FVeyraPlacedShape& Placed)
{
	TArray<FVector2D> Points;
	const FVector2D Origin(Placed.Origin);
	const FVector Along3 = Placed.Direction.GetSafeNormal2D().IsZero() ? FVector::ForwardVector : Placed.Direction.GetSafeNormal2D();
	const FVector2D Along(Along3);
	const FVector2D Across(-Along.Y, Along.X);
	const FVeyraShape& Shape = Placed.Shape;
	Points.Add(Origin);
	if (Shape.Kind == EVeyraShapeKind::Rectangle)
	{
		for (int32 Step = 0; Step <= FootprintRimPoints; ++Step)
		{
			const double U = Shape.Length * Step / FootprintRimPoints;
			for (int32 Side = 0; Side < FootprintAcross; ++Side)
			{
				const double V = Shape.Width * (static_cast<double>(Side) / (FootprintAcross - 1) - 0.5);
				Points.Add(Origin + Along * U + Across * V);
			}
		}
		return Points;
	}
	const double HalfArc = Shape.Kind == EVeyraShapeKind::Sector ? FMath::DegreesToRadians(Shape.ArcDegrees) / 2.0 : UE_DOUBLE_PI;
	for (int32 Step = 0; Step < FootprintRimPoints; ++Step)
	{
		const double Share = Shape.Kind == EVeyraShapeKind::Sector ? static_cast<double>(Step) / (FootprintRimPoints - 1) : static_cast<double>(Step) / FootprintRimPoints;
		const double Angle = -HalfArc + 2.0 * HalfArc * Share;
		const FVector2D Way = Along * FMath::Cos(Angle) + Across * FMath::Sin(Angle);
		Points.Add(Origin + Way * Shape.Radius);
		Points.Add(Origin + Way * (Shape.Radius / 2.0));
	}
	return Points;
}

FVeyraGroundFit VeyraTelegraphFill::FitGround(const FVeyraPlacedShape& Placed, TFunctionRef<double(const FVector2D&)> HeightAt)
{
	// Least squares: the ground's height z = A x + B y + C about the area's origin.
	const TArray<FVector2D> Points = FootprintOf(Placed);
	const FVector2D Origin(Placed.Origin);
	double Sxx = 0, Sxy = 0, Syy = 0, Sx = 0, Sy = 0, Sz = 0, Sxz = 0, Syz = 0;
	TArray<FVector> Samples;
	for (const FVector2D& Point : Points)
	{
		const FVector2D D = Point - Origin;
		const double Z = HeightAt(Point);
		Samples.Add(FVector(D.X, D.Y, Z));
		Sxx += D.X * D.X; Sxy += D.X * D.Y; Syy += D.Y * D.Y; Sx += D.X; Sy += D.Y; Sz += Z; Sxz += D.X * Z; Syz += D.Y * Z;
	}
	const double N = Samples.Num();
	const FMatrix Normal(FPlane(Sxx, Sxy, Sx, 0.0), FPlane(Sxy, Syy, Sy, 0.0), FPlane(Sx, Sy, N, 0.0), FPlane(0.0, 0.0, 0.0, 1.0));
	double A = 0.0, B = 0.0, C = N > 0.0 ? Sz / N : Placed.Origin.Z;
	if (FMath::Abs(Normal.Determinant()) > UE_KINDA_SMALL_NUMBER)
	{
		const FMatrix Inverse = Normal.Inverse();
		const FVector Solved = Inverse.TransformVector(FVector(Sxz, Syz, Sz));
		A = Solved.X;
		B = Solved.Y;
		C = Solved.Z;
	}
	FVeyraGroundFit Fit;
	Fit.Origin = FVector(Origin, C);
	Fit.Normal = FVector(-A, -B, 1.0).GetSafeNormal();
	for (const FVector& Sample : Samples)
	{
		Fit.MaxStray = FMath::Max(Fit.MaxStray, FMath::Abs(Sample.Z - (A * Sample.X + B * Sample.Y + C)));
	}
	return Fit;
}

EVeyraTelegraphShow VeyraTelegraphFill::ShowOverGround(EVeyraTelegraphShow Show, const FVeyraGroundFit& Fit, double Tolerance)
{
	return Show == EVeyraTelegraphShow::None && Fit.MaxStray > Tolerance ? EVeyraTelegraphShow::Outline : Show;
}

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
