// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Terrain/VeyraRuntimeTerrain.h"
#include "Terrain/VeyraTerrainBox.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCombatTests
{
	// Veyra.Combat.TerrainBox.*: a wall's footprint as pure geometry (ADR-043 §1, §3), which the
	// layout's validation and sight both measure against.
	TEST_CLASS(TerrainBox, "Veyra.Combat")
	{
		static constexpr double Tolerance = 1e-6;

		/** Fixture: a wall facing +X, 100 deep along X and 400 long across it, at the origin. */
		static FVeyraTerrainBox Wall()
		{
			return { FVector2D::ZeroVector, FVector2D(1.0, 0.0), 400.0, 100.0 };
		}

		TEST_METHOD(ItsLengthRunsAcrossTheWayItFaces)
		{
			const FVeyraTerrainBox Box = Wall();
			const FBox2D Bounds = Box.Bounds();
			ASSERT_THAT(IsTrue(Bounds.Min.Equals(FVector2D(-50.0, -200.0), Tolerance) && Bounds.Max.Equals(FVector2D(50.0, 200.0), Tolerance)));
			FVeyraWallRequest Request;
			Request.Centre = FVector(10.0, 20.0, 90.0);
			Request.Facing = FVector(0.0, 1.0, 0.0);
			Request.Length = 400.0;
			Request.Thickness = 100.0;
			const FBox2D Raised = FVeyraTerrainBox::Of(Request).Bounds();
			ASSERT_THAT(IsTrue(Raised.Min.Equals(FVector2D(-190.0, -30.0), Tolerance) && Raised.Max.Equals(FVector2D(210.0, 70.0), Tolerance), TEXT("a raised wall's footprint")));
		}

		TEST_METHOD(APointsDistanceIsToTheNearestEdgeOrCorner)
		{
			const FVeyraTerrainBox Box = Wall();
			ASSERT_THAT(IsTrue(Box.DistanceTo(FVector2D(0.0, 0.0)) == 0.0, TEXT("inside")));
			ASSERT_THAT(IsTrue(Box.DistanceTo(FVector2D(50.0, 200.0)) == 0.0, TEXT("on a corner")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Box.DistanceTo(FVector2D(80.0, 0.0)), 30.0, Tolerance), TEXT("off its face")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Box.DistanceTo(FVector2D(0.0, -260.0)), 60.0, Tolerance), TEXT("off its end")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Box.DistanceTo(FVector2D(53.0, 204.0)), 5.0, Tolerance), TEXT("off a corner")));
		}

		TEST_METHOD(ASegmentCrossesItOrPassesBy)
		{
			const FVeyraTerrainBox Box = Wall();
			ASSERT_THAT(IsTrue(Box.Crosses(FVector2D(-100.0, 0.0), FVector2D(100.0, 0.0)), TEXT("straight through")));
			ASSERT_THAT(IsTrue(Box.Crosses(FVector2D(-100.0, -300.0), FVector2D(100.0, 300.0)), TEXT("on a slant")));
			ASSERT_THAT(IsTrue(Box.Crosses(FVector2D(0.0, 0.0), FVector2D(0.0, 10.0)), TEXT("wholly inside")));
			ASSERT_THAT(IsFalse(Box.Crosses(FVector2D(-100.0, 300.0), FVector2D(100.0, 300.0)), TEXT("beyond its end")));
			ASSERT_THAT(IsFalse(Box.Crosses(FVector2D(60.0, -300.0), FVector2D(60.0, 300.0)), TEXT("along its face, just clear")));
			ASSERT_THAT(IsFalse(Box.Crosses(FVector2D(-200.0, 0.0), FVector2D(-80.0, 0.0)), TEXT("stopping short")));
			ASSERT_THAT(IsTrue(Box.DistanceToSegment(FVector2D(-100.0, 0.0), FVector2D(100.0, 0.0)) == 0.0));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Box.DistanceToSegment(FVector2D(60.0, -300.0), FVector2D(60.0, 300.0)), 10.0, Tolerance)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Box.DistanceToSegment(FVector2D(-200.0, 0.0), FVector2D(-80.0, 0.0)), 30.0, Tolerance)));
			// A corner is the nearest point to a segment that passes it on a slant.
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Box.DistanceToSegment(FVector2D(150.0, 200.0), FVector2D(50.0, 300.0)), 100.0 / UE_SQRT_2, 1e-3)));
		}

		TEST_METHOD(ATurnedWallMeasuresInItsOwnFrame)
		{
			// Facing along the diagonal, so its length runs along the other diagonal.
			const FVeyraTerrainBox Box{ FVector2D(100.0, 100.0), FVector2D(1.0, 1.0), 400.0, 100.0 };
			const FVector2D Along = FVector2D(1.0, 1.0).GetSafeNormal();
			const FVector2D Across(-Along.Y, Along.X);
			ASSERT_THAT(IsTrue(Box.DistanceTo(FVector2D(100.0, 100.0) + Across * 199.0) == 0.0, TEXT("within its length")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Box.DistanceTo(FVector2D(100.0, 100.0) + Along * 80.0), 30.0, Tolerance), TEXT("off its face")));
			ASSERT_THAT(IsTrue(Box.Crosses(FVector2D(100.0, 100.0) - Along * 300.0, FVector2D(100.0, 100.0) + Along * 300.0)));
			ASSERT_THAT(IsFalse(Box.Crosses(FVector2D(100.0, 100.0) + Across * 250.0 - Along * 300.0, FVector2D(100.0, 100.0) + Across * 250.0 + Along * 300.0)));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
