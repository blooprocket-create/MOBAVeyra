// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Greybox/VeyraLimbIK.h"

namespace VeyraLimbIKTests
{
	// Veyra.UI.LimbIK.*: the inverse kinematics that hold a drawn limb to something fixed (ADR-069).
	TEST_CLASS(LimbIK, "Veyra.UI")
	{
		// Fixture values: a leg of two 40 cm bones standing straight, its knee pointing forward (+X), and a tolerance.
		static constexpr float Bone = 40.0f;
		static constexpr double Tolerance = 0.05;

		static VeyraLimbIK::FChain Leg()
		{
			VeyraLimbIK::FChain Chain;
			Chain.Root = FVector(0.0, 0.0, 80.0);
			Chain.Joint = FVector(1.0, 0.0, 40.0);
			Chain.End = FVector(0.0, 0.0, 0.0);
			return Chain;
		}

		TEST_METHOD(AReachableTargetIsReachedWithTheBonesTheirLength)
		{
			const VeyraLimbIK::FChain Chain = Leg();
			const FVector Target(10.0, 5.0, 15.0);
			const VeyraLimbIK::FChain Solved = VeyraLimbIK::Solve(Chain, Target, FVector(100.0, 0.0, 40.0), 0.08f);
			ASSERT_THAT(IsTrue(Solved.Root.Equals(Chain.Root), TEXT("the hip stays")));
			ASSERT_THAT(IsTrue(Solved.End.Equals(Target, Tolerance), TEXT("the foot reaches its ground")));
			ASSERT_THAT(IsNear(FVector::Dist(Solved.Root, Solved.Joint), FVector::Dist(Chain.Root, Chain.Joint), Tolerance, TEXT("the thigh keeps its length")));
			ASSERT_THAT(IsNear(FVector::Dist(Solved.Joint, Solved.End), FVector::Dist(Chain.Joint, Chain.End), Tolerance, TEXT("and the shin")));
		}

		TEST_METHOD(TheJointBendsTowardItsPoleAndNeverFlips)
		{
			const VeyraLimbIK::FChain Chain = Leg();
			const FVector Target(0.0, 0.0, 30.0);
			const VeyraLimbIK::FChain Forward = VeyraLimbIK::Solve(Chain, Target, FVector(100.0, 0.0, 40.0), 0.08f);
			ASSERT_THAT(IsTrue(Forward.Joint.X > 5.0, TEXT("a knee with its pole ahead bends forward")));
			const VeyraLimbIK::FChain Back = VeyraLimbIK::Solve(Chain, Target, FVector(-100.0, 0.0, 40.0), 0.08f);
			ASSERT_THAT(IsTrue(Back.Joint.X < -5.0, TEXT("and with it behind, backward")));
			// A pole on the line itself leaves the joint bending as it bent.
			const VeyraLimbIK::FChain OnLine = VeyraLimbIK::Solve(Chain, Target, FVector(0.0, 0.0, 60.0), 0.08f);
			ASSERT_THAT(IsTrue(OnLine.Joint.X > 0.0, TEXT("as it bent")));
		}

		TEST_METHOD(BeyondReachTheBonesStretchALittleThenStop)
		{
			const VeyraLimbIK::FChain Chain = Leg();
			const float Length = static_cast<float>(FVector::Dist(Chain.Root, Chain.Joint) + FVector::Dist(Chain.Joint, Chain.End));
			const float MaxStretch = 0.08f;
			// Just past reach: stretched to it.
			const FVector Near = Chain.Root - FVector(0.0, 0.0, Length * 1.04f);
			ASSERT_THAT(IsTrue(VeyraLimbIK::Solve(Chain, Near, FVector(100.0, 0.0, 40.0), MaxStretch).End.Equals(Near, 0.2f), TEXT("eased to it")));
			// Far past: as far as the stretched chain goes, no farther.
			const FVector Far = Chain.Root - FVector(0.0, 0.0, Length * 2.0f);
			const VeyraLimbIK::FChain Solved = VeyraLimbIK::Solve(Chain, Far, FVector(100.0, 0.0, 40.0), MaxStretch);
			ASSERT_THAT(IsNear(FVector::Dist(Solved.Root, Solved.End), static_cast<double>(Length * (1.0f + MaxStretch)), 0.2, TEXT("stretched at most MaxStretch")));
		}

		TEST_METHOD(AFootIsPlantedOnItsRestHeightAndFreedAsItLifts)
		{
			const float Fade = 8.0f;
			ASSERT_THAT(IsNear(VeyraLimbIK::PlantWeight(0.0f, Fade), 1.0f, 1e-6f, TEXT("planted")));
			ASSERT_THAT(IsNear(VeyraLimbIK::PlantWeight(-2.0f, Fade), 1.0f, 1e-6f, TEXT("below its rest, still planted")));
			ASSERT_THAT(IsNear(VeyraLimbIK::PlantWeight(Fade, Fade), 0.0f, 1e-6f, TEXT("lifted, free")));
			const float Half = VeyraLimbIK::PlantWeight(Fade * 0.5f, Fade);
			ASSERT_THAT(IsTrue(Half > 0.0f && Half < 1.0f, TEXT("eased between")));
		}

		TEST_METHOD(ThePelvisLowersForTheLowerFootButNeverRises)
		{
			ASSERT_THAT(IsNear(VeyraLimbIK::PelvisDrop(-12.0f, 1.0f, 4.0f, 1.0f, 30.0f), -12.0f, 1e-6f, TEXT("to the lower foot's ground")));
			ASSERT_THAT(IsNear(VeyraLimbIK::PelvisDrop(-12.0f, 0.5f, 0.0f, 1.0f, 30.0f), -6.0f, 1e-6f, TEXT("as much as that foot is planted")));
			ASSERT_THAT(IsNear(VeyraLimbIK::PelvisDrop(10.0f, 1.0f, 6.0f, 1.0f, 30.0f), 0.0f, 1e-6f, TEXT("never up")));
			ASSERT_THAT(IsNear(VeyraLimbIK::PelvisDrop(-80.0f, 1.0f, 0.0f, 1.0f, 30.0f), -30.0f, 1e-6f, TEXT("no lower than its most")));
		}

		TEST_METHOD(AHandIsHeldNearItsGripAndLetGoFarFromIt)
		{
			const float Release = 25.0f;
			ASSERT_THAT(IsNear(VeyraLimbIK::HoldWeight(0.0f, Release), 1.0f, 1e-6f, TEXT("on the grip, held")));
			ASSERT_THAT(IsNear(VeyraLimbIK::HoldWeight(Release, Release), 0.0f, 1e-6f, TEXT("taken away, let go")));
			const float Drift = VeyraLimbIK::HoldWeight(2.0f, Release);
			ASSERT_THAT(IsTrue(Drift > 0.95f && Drift < 1.0f, TEXT("a small drift, still held")));
		}

		TEST_METHOD(AMovingBodyKeepsLessOfTheSolve)
		{
			ASSERT_THAT(IsNear(VeyraLimbIK::SpeedWeight(0.0f, 300.0f, 0.6f), 1.0f, 1e-6f, TEXT("standing, all")));
			ASSERT_THAT(IsNear(VeyraLimbIK::SpeedWeight(300.0f, 300.0f, 0.6f), 0.6f, 1e-6f, TEXT("at full speed, its moving share")));
			ASSERT_THAT(IsNear(VeyraLimbIK::SpeedWeight(150.0f, 300.0f, 0.6f), 0.8f, 1e-6f, TEXT("between, between")));
		}
	};
}

#endif
