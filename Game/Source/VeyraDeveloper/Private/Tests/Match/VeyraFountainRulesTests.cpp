// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Rules/VeyraMatchRules.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraFountainRulesTests
{
	// Veyra.Match.FountainRules.*: where a move order takes a Vanguard during fountain preparation (Match Flow Bible
	// §1; ADR-054 §1). Fixture values, not the committed tuning.
	TEST_CLASS(FountainRules, "Veyra.Match")
	{
		static constexpr double Radius = 600.0;
		const FVector Fountain = FVector(-5000.0, -5000.0, 100.0);

		TEST_METHOD(AnOrderInsideTheFountainStandsAsGiven)
		{
			const FVector Inside = Fountain + FVector(300.0, -200.0, 25.0);
			ASSERT_THAT(IsTrue(VeyraMatchRules::ClampToFountain(Inside, Fountain, Radius).Equals(Inside)));
			const FVector OnTheEdge = Fountain + FVector(0.0, Radius, 0.0);
			ASSERT_THAT(IsTrue(VeyraMatchRules::ClampToFountain(OnTheEdge, Fountain, Radius).Equals(OnTheEdge), TEXT("the edge is inside")));
		}

		TEST_METHOD(AnOrderBeyondItStopsAtTheEdgeTowardWhereItPointed)
		{
			const FVector Beyond = Fountain + FVector(3000.0, 4000.0, -40.0);
			const FVector Clamped = VeyraMatchRules::ClampToFountain(Beyond, Fountain, Radius);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FVector::Dist2D(Clamped, Fountain), Radius, 0.01), TEXT("on the circle")));
			// 3-4-5: the nearest point of the circle lies along the order's own line.
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Clamped.X, Fountain.X + Radius * 0.6, 0.01) && FMath::IsNearlyEqual(Clamped.Y, Fountain.Y + Radius * 0.8, 0.01)));
			ASSERT_THAT(IsTrue(Clamped.Z == Beyond.Z, TEXT("the ground point's own height")));
		}
	};
}

#endif