// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Input/VeyraOrderPrediction.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraOrderPredictionTests
{
	// Veyra.Match.OrderPrediction.*: the motion the player's own machine draws an order it gives by, until the server answers
	// it (ADR-079 §5). Fixture values, not the committed tuning.
	TEST_CLASS(OrderPrediction, "Veyra.Match")
	{
		static constexpr double Speed = 350.0;
		static constexpr double StopShort = 10.0;
		static constexpr double Far = 500.0;
		static constexpr double Now = 42.0;
		static constexpr uint16 Number = 7;
		static constexpr double Tolerance = 1e-3;

		static FVeyraOrderPredictionInputs Inputs()
		{
			FVeyraOrderPredictionInputs In;
			In.From = FVector(0.0, 0.0, 90.0);
			In.Speed = Speed;
			In.Goal = FVector(Far, 0.0, 0.0);
			In.StopShort = StopShort;
			In.bMayWalk = true;
			In.ServerNow = Now;
			In.OrderNumber = Number;
			return In;
		}

		TEST_METHOD(AMoveWalksStraightToWhereTheServerWillStopIt)
		{
			const TOptional<FVeyraMotion> Predicted = VeyraOrderPrediction::Predict(Inputs());
			ASSERT_THAT(IsTrue(Predicted.IsSet() && Predicted->Kind == EVeyraMotionKind::Walk));
			ASSERT_THAT(AreEqual(Predicted->OrderNumber, Number));
			ASSERT_THAT(IsNear(Predicted->StartTime, Now, Tolerance));
			ASSERT_THAT(IsNear(static_cast<double>(Predicted->Speed), Speed, Tolerance));
			ASSERT_THAT(AreEqual(Predicted->Corners.Num(), 1));
			ASSERT_THAT(IsTrue(FVector(Predicted->Corners[0]).Equals(FVector(Far - StopShort, 0.0, 90.0), 1.0), TEXT("short of the goal, at its own height")));
		}

		TEST_METHOD(ABodyThatMayNotWalkPredictsNothing)
		{
			FVeyraOrderPredictionInputs In = Inputs();
			In.bMayWalk = false;
			ASSERT_THAT(IsFalse(VeyraOrderPrediction::Predict(In).IsSet()));
		}

		TEST_METHOD(AMoveAlreadyThereShowsNothing)
		{
			FVeyraOrderPredictionInputs In = Inputs();
			In.Goal = In.From + FVector(StopShort * 0.5, 0.0, 0.0);
			ASSERT_THAT(IsFalse(VeyraOrderPrediction::Predict(In).IsSet()));
		}

		TEST_METHOD(AnAttackWithinReachTurnsToItsTarget)
		{
			FVeyraOrderPredictionInputs In = Inputs();
			In.Goal = FVector(0.0, Far, 0.0);
			In.StopShort = Far * 2.0;
			In.bFacesWithinReach = true;
			const TOptional<FVeyraMotion> Predicted = VeyraOrderPrediction::Predict(In);
			ASSERT_THAT(IsTrue(Predicted.IsSet() && Predicted->Kind == EVeyraMotionKind::Stand));
			ASSERT_THAT(IsNear(static_cast<double>(Predicted->Yaw), 90.0, Tolerance));
		}
	};
}

#endif
