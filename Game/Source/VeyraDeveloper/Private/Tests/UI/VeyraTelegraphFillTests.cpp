// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Greybox/VeyraTelegraphFill.h"

namespace VeyraTelegraphFillTests
{
	// Veyra.UI.TelegraphFill.*: the shaded ground under a threatening or aimed telegraph (ADR-068 §4).
	TEST_CLASS(TelegraphFill, "Veyra.UI")
	{
		// Fixture values: a shape's sizes, and the landing window.
		static constexpr double Radius = 300.0;
		static constexpr double Arc = 90.0;
		static constexpr double Length = 800.0;
		static constexpr double Width = 120.0;
		static constexpr double LandingSeconds = 0.6;

		static FVeyraPlacedShape Placed(EVeyraShapeKind Kind)
		{
			FVeyraPlacedShape Shape;
			Shape.Shape.Kind = Kind;
			Shape.Shape.Radius = Kind == EVeyraShapeKind::Rectangle ? 0.0 : Radius;
			Shape.Shape.ArcDegrees = Kind == EVeyraShapeKind::Sector ? Arc : 0.0;
			Shape.Shape.Length = Kind == EVeyraShapeKind::Rectangle ? Length : 0.0;
			Shape.Shape.Width = Kind == EVeyraShapeKind::Rectangle ? Width : 0.0;
			Shape.Direction = FVector::RightVector;
			return Shape;
		}

		TEST_METHOD(EachShapeLiesOnAQuadFittedToIt)
		{
			const FVeyraTelegraphFill Circle = VeyraTelegraphFill::Of(Placed(EVeyraShapeKind::Circle), FVector::ZeroVector);
			ASSERT_THAT(IsTrue(Circle.Shape == 0 && Circle.HalfSize.Equals(FVector2D(Radius)) && Circle.Centre.IsZero()));
			const FVeyraTelegraphFill Sector = VeyraTelegraphFill::Of(Placed(EVeyraShapeKind::Sector), FVector::ZeroVector);
			ASSERT_THAT(IsTrue(Sector.Shape == 1 && FMath::IsNearlyEqual(Sector.HalfArc, FMath::DegreesToRadians(Arc) / 2.0)));
			ASSERT_THAT(IsNear(Sector.Yaw, 90.0, 1e-6, TEXT("facing its direction")));
			// A rectangle runs from its origin along its direction.
			const FVeyraTelegraphFill Lane = VeyraTelegraphFill::Of(Placed(EVeyraShapeKind::Rectangle), FVector::ZeroVector);
			ASSERT_THAT(IsTrue(Lane.Shape == 2 && Lane.HalfSize.Equals(FVector2D(Length / 2.0, Width / 2.0))));
			ASSERT_THAT(IsTrue(Lane.Centre.Equals(FVector(0.0, Length / 2.0, 0.0), 1e-6)));
		}

		TEST_METHOD(OnlyWhatThreatensOrAimsIsFilled)
		{
			for (const EVeyraTelegraphSource Source : { EVeyraTelegraphSource::Windup, EVeyraTelegraphSource::Channel, EVeyraTelegraphSource::DelayedArea,
					 EVeyraTelegraphSource::LingeringArea, EVeyraTelegraphSource::LingeringAreaEnding, EVeyraTelegraphSource::Indicator,
					 EVeyraTelegraphSource::ProjectileLane })
			{
				ASSERT_THAT(IsTrue(VeyraTelegraphFill::IsFilled(Source)));
			}
			ASSERT_THAT(IsFalse(VeyraTelegraphFill::IsFilled(EVeyraTelegraphSource::Selection), TEXT("a mark, not a threat")));
			ASSERT_THAT(IsFalse(VeyraTelegraphFill::IsFilled(EVeyraTelegraphSource::AttackRange), TEXT("a guide over a wide ground")));
		}

		TEST_METHOD(WhatLandsFillsInOverItsLastMoments)
		{
			using namespace VeyraTelegraphFill;
			ASSERT_THAT(IsNear(LandingOf(EVeyraTelegraphSource::Windup, LandingSeconds * 2.0, LandingSeconds), 0.0, 1e-9, TEXT("not yet")));
			ASSERT_THAT(IsNear(LandingOf(EVeyraTelegraphSource::DelayedArea, LandingSeconds / 2.0, LandingSeconds), 0.5, 1e-9, TEXT("half way")));
			ASSERT_THAT(IsNear(LandingOf(EVeyraTelegraphSource::Channel, 0.0, LandingSeconds), 1.0, 1e-9, TEXT("landing")));
			ASSERT_THAT(IsNear(LandingOf(EVeyraTelegraphSource::Indicator, 0.0, LandingSeconds), 0.0, 1e-9, TEXT("an aim never lands")));
			ASSERT_THAT(IsNear(LandingOf(EVeyraTelegraphSource::LingeringArea, 0.0, LandingSeconds), 0.0, 1e-9, TEXT("a lingering area lands only as it ends")));
			ASSERT_THAT(IsNear(LandingOf(EVeyraTelegraphSource::LingeringAreaEnding, 0.0, LandingSeconds), 1.0, 1e-9));
		}
	};
}

#endif
