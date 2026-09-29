// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Camera/VeyraCameraRules.h"
#include "CQTest.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCameraTests
{
	// Veyra.Match.LocalCamera.*: the local camera's rules (Settings Bible §2; ADR-020 §1). Fixture values.
	TEST_CLASS(LocalCamera, "Veyra.Match")
	{
		FVeyraCameraLimits Limits;

		FVector Step(const FVector& Focus, EVeyraCameraMode Mode, const FVeyraCameraInput& Input, double Seconds, FVector* InOutOffset = nullptr) const
		{
			FVeyraCameraState State;
			State.Focus = Focus;
			State.Offset = InOutOffset ? *InOutOffset : FVector::ZeroVector;
			const FVeyraCameraState Next = VeyraCamera::Step(State, Mode, Input, Limits, Seconds);
			if (InOutOffset)
			{
				*InOutOffset = Next.Offset;
			}
			return Next.Focus;
		}

		BEFORE_EACH()
		{
			Limits.PanSpeed = 1000.0;
			Limits.SemiLockedMaxOffset = 500.0;
			Limits.HalfExtent = 5000.0;
		}

		TEST_METHOD(AFreeCameraPansWhereItIsToldAndStaysOnTheMap)
		{
			FVeyraCameraInput Input;
			Input.Pan = FVector2D(1.0, 0.0);
			Input.Vanguard = FVector(0.0, 0.0, 0.0);
			const FVector Right = Step(FVector::ZeroVector, EVeyraCameraMode::Free, Input, 0.5);
			ASSERT_THAT(IsTrue(Right.Equals(FVector(0.0, 500.0, 0.0)), TEXT("the screen's right is +Y, and the Vanguard does not pull it")));
			Input.Pan = FVector2D(0.0, 1.0);
			ASSERT_THAT(IsTrue(Step(FVector::ZeroVector, EVeyraCameraMode::Free, Input, 0.5).Equals(FVector(500.0, 0.0, 0.0)), TEXT("its top is +X")));
			ASSERT_THAT(IsTrue(Step(FVector(4900.0, 0.0, 0.0), EVeyraCameraMode::Free, Input, 1.0).X == Limits.HalfExtent, TEXT("never off the map")));
		}

		TEST_METHOD(ALockedCameraFollowsAndHoldToCenterDoesInAnyMode)
		{
			FVeyraCameraInput Input;
			Input.Pan = FVector2D(1.0, 1.0);
			Input.Vanguard = FVector(300.0, -200.0, 50.0);
			ASSERT_THAT(IsTrue(Step(FVector::ZeroVector, EVeyraCameraMode::Locked, Input, 1.0).Equals(Input.Vanguard.GetValue())));
			Input.bHoldCenter = true;
			ASSERT_THAT(IsTrue(Step(FVector(2000.0, 2000.0, 0.0), EVeyraCameraMode::Free, Input, 1.0).Equals(Input.Vanguard.GetValue())));
		}

		TEST_METHOD(ASemiLockedCameraLooksOnlyALimitedWayOff)
		{
			FVeyraCameraInput Input;
			Input.Pan = FVector2D(0.0, 1.0);
			Input.Vanguard = FVector(1000.0, 1000.0, 0.0);
			FVector Offset = FVector::ZeroVector;
			const FVector Looked = Step(FVector(1000.0, 1000.0, 0.0), EVeyraCameraMode::SemiLocked, Input, 2.0, &Offset);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FVector::Dist2D(Looked, Input.Vanguard.GetValue()), Limits.SemiLockedMaxOffset), TEXT("up to its limit")));
			// The Vanguard walks on; the camera keeps its offset.
			Input.Pan = FVector2D::ZeroVector;
			Input.Vanguard = FVector(1200.0, 1000.0, 0.0);
			const FVector Followed = Step(Looked, EVeyraCameraMode::SemiLocked, Input, 0.1, &Offset);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FVector::Dist2D(Followed, Input.Vanguard.GetValue()), Limits.SemiLockedMaxOffset)));
		}

		TEST_METHOD(TheEdgesPanAndTheModeKeyCycles)
		{
			const FVector2D Screen(1920.0, 1080.0);
			ASSERT_THAT(IsTrue(VeyraCamera::EdgePan(FVector2D(0.0, 540.0), Screen, 10.0).Equals(FVector2D(-1.0, 0.0))));
			ASSERT_THAT(IsTrue(VeyraCamera::EdgePan(FVector2D(960.0, 0.0), Screen, 10.0).Equals(FVector2D(0.0, 1.0)), TEXT("the top pans up")));
			ASSERT_THAT(IsTrue(VeyraCamera::EdgePan(FVector2D(960.0, 540.0), Screen, 10.0).IsZero()));
			ASSERT_THAT(IsTrue(VeyraCamera::Next(EVeyraCameraMode::Free) == EVeyraCameraMode::Locked));
			ASSERT_THAT(IsTrue(VeyraCamera::Next(EVeyraCameraMode::Locked) == EVeyraCameraMode::SemiLocked));
			ASSERT_THAT(IsTrue(VeyraCamera::Next(EVeyraCameraMode::SemiLocked) == EVeyraCameraMode::Free));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
