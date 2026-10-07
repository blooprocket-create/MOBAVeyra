// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Greybox/VeyraSpringChain.h"

namespace VeyraSpringChainTests
{
	// Veyra.UI.SpringChain.*: the secondary motion of a body's loose parts, a cloak or a lock of hair (ADR-069).
	TEST_CLASS(SpringChain, "Veyra.UI")
	{
		// Fixture values: a chain of three 20 cm bones hanging straight down from 100 cm up; a frame of the world; the
		// spring it moves by; how it keeps time (a hitch's longest step, a substep, a teleport's shortest jump); a run's
		// speed (cm per second).
		static constexpr double Bone = 20.0;
		static constexpr double Top = 100.0;
		static constexpr float Frame = 1.0f / 30.0f;
		static constexpr double Speed = 360.0;
		static constexpr double Tolerance = 0.5;

		static TArray<FVector> Hanging(const FVector& Root = FVector(0.0, 0.0, Top))
		{
			return { Root, Root - FVector(0, 0, Bone), Root - FVector(0, 0, Bone * 2.0), Root - FVector(0, 0, Bone * 3.0) };
		}

		static VeyraSpringChain::FParams Spring()
		{
			VeyraSpringChain::FParams Params;
			Params.Stiffness = 60.0f;
			Params.Drag = 6.0f;
			Params.Damping = 4.0f;
			Params.MaxAngleDegrees = 80.0f;
			return Params;
		}

		static VeyraSpringChain::FTiming Timing()
		{
			return { 0.1f, 1.0f / 120.0f, 200.0f };
		}

		static void Run(VeyraSpringChain::FState& State, TConstArrayView<FVector> Animated, float Seconds, const VeyraSpringChain::FParams& Params,
			TConstArrayView<VeyraSpringChain::FCollider> Colliders = {}, float Step = Frame)
		{
			for (float Elapsed = 0.0f; Elapsed < Seconds; Elapsed += Step)
			{
				VeyraSpringChain::Step(State, Animated, Step, Params, Colliders, Timing());
			}
		}

		/** The chain carried along +X at Speed for Seconds in frames of Step; how far its tip trails its clip's. */
		static double TrailAfterRunning(const VeyraSpringChain::FParams& Params, float Seconds, float Step)
		{
			VeyraSpringChain::FState State = VeyraSpringChain::AtRest(Hanging());
			const int32 Frames = FMath::RoundToInt32(Seconds / Step);
			TArray<FVector> Clip;
			for (int32 Index = 1; Index <= Frames; ++Index)
			{
				Clip = Hanging(FVector(Speed * Step * Index, 0.0, Top));
				VeyraSpringChain::Step(State, Clip, Step, Params, {}, Timing());
			}
			return Clip.Last().X - State.Points.Last().X;
		}

		TEST_METHOD(AStillChainRestsWhereItsClipHasIt)
		{
			const TArray<FVector> Clip = Hanging();
			VeyraSpringChain::FState State = VeyraSpringChain::AtRest(Clip);
			Run(State, Clip, 2.0f, Spring());
			for (int32 Joint = 0; Joint < Clip.Num(); ++Joint)
			{
				ASSERT_THAT(IsTrue(State.Points[Joint].Equals(Clip[Joint], Tolerance), TEXT("at rest on its clip")));
			}
		}

		TEST_METHOD(AMovedRootLeavesItsTipBehindThenItCatchesUp)
		{
			VeyraSpringChain::FState State = VeyraSpringChain::AtRest(Hanging());
			// The body steps sideways (+Y) and stops.
			const TArray<FVector> Moved = Hanging(FVector(0.0, 30.0, Top));
			VeyraSpringChain::Step(State, Moved, Frame, Spring(), {}, Timing());
			ASSERT_THAT(IsTrue(State.Points.Last().Y < Moved.Last().Y - 5.0, TEXT("its tip trails behind")));
			Run(State, Moved, 3.0f, Spring());
			ASSERT_THAT(IsTrue(State.Points.Last().Equals(Moved.Last(), Tolerance), TEXT("then settles under it")));
		}

		TEST_METHOD(ARunningBodyStreamsItBehindAlikeAtAnyFrameRate)
		{
			// Drag through the air trails a running body's cloak; stepped in substeps, as far at 144 frames a second as at 30.
			const double AtThirty = TrailAfterRunning(Spring(), 2.0f, 1.0f / 30.0f);
			const double AtFast = TrailAfterRunning(Spring(), 2.0f, 1.0f / 144.0f);
			ASSERT_THAT(IsTrue(AtThirty > Bone * 0.25, *FString::Printf(TEXT("it streams behind: %.2f"), AtThirty)));
			ASSERT_THAT(IsNear(AtFast, AtThirty, AtThirty * 0.05, *FString::Printf(TEXT("alike: %.2f at 30, %.2f at 144"), AtThirty, AtFast)));
		}

		TEST_METHOD(WithoutDragItRidesAlongWithItsClip)
		{
			// Damping alone (a lock of hair) settles it relative to its clip, so a steady run leaves it where its clip has it.
			VeyraSpringChain::FParams Params = Spring();
			Params.Drag = 0.0f;
			ASSERT_THAT(IsNear(TrailAfterRunning(Params, 3.0f, Frame), 0.0, Tolerance, TEXT("no trail without drag")));
		}

		TEST_METHOD(EveryBoneKeepsItsLength)
		{
			VeyraSpringChain::FState State = VeyraSpringChain::AtRest(Hanging());
			Run(State, Hanging(FVector(40.0, -25.0, Top + 10.0)), 0.5f, Spring());
			for (int32 Joint = 1; Joint < State.Points.Num(); ++Joint)
			{
				ASSERT_THAT(IsNear(FVector::Dist(State.Points[Joint], State.Points[Joint - 1]), Bone, 1e-3));
			}
		}

		TEST_METHOD(ItNeverTurnsFurtherThanItsLimitFromItsClip)
		{
			VeyraSpringChain::FState State = VeyraSpringChain::AtRest(Hanging());
			VeyraSpringChain::FParams Params = Spring();
			Params.MaxAngleDegrees = 30.0f;
			// A violent jerk sideways, as a dash would.
			const TArray<FVector> Jerked = Hanging(FVector(0.0, 150.0, Top));
			VeyraSpringChain::Step(State, Jerked, Frame, Params, {}, Timing());
			for (int32 Joint = 1; Joint < State.Points.Num(); ++Joint)
			{
				const FVector Simulated = (State.Points[Joint] - State.Points[Joint - 1]).GetSafeNormal();
				const double Degrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Simulated, FVector(0, 0, -1)), -1.0, 1.0)));
				ASSERT_THAT(IsTrue(Degrees <= 30.0 + 1e-3, *FString::Printf(TEXT("bone %d turned %.2f"), Joint, Degrees)));
			}
		}

		TEST_METHOD(ItHangsOutsideTheBodyNotThroughIt)
		{
			// A torso just behind the chain; the clip pulls the chain back into it.
			const VeyraSpringChain::FCollider Torso{ FVector(-15.0, 0.0, 0.0), FVector(-15.0, 0.0, Top), 12.0f };
			TArray<FVector> Clip = Hanging();
			for (int32 Joint = 1; Joint < Clip.Num(); ++Joint)
			{
				Clip[Joint].X -= 8.0 * Joint;
			}
			VeyraSpringChain::FState State = VeyraSpringChain::AtRest(Hanging());
			const VeyraSpringChain::FCollider Colliders[] = { Torso };
			Run(State, Clip, 2.0f, Spring(), Colliders);
			for (int32 Joint = 1; Joint < State.Points.Num(); ++Joint)
			{
				const double FromAxis = FVector2D(State.Points[Joint].X + 15.0, State.Points[Joint].Y).Size();
				ASSERT_THAT(IsTrue(FromAxis >= 12.0 - Tolerance, *FString::Printf(TEXT("joint %d inside by %.2f"), Joint, 12.0 - FromAxis)));
			}
		}

		TEST_METHOD(ATeleportSettlesItOnItsClip)
		{
			VeyraSpringChain::FState State = VeyraSpringChain::AtRest(Hanging());
			const TArray<FVector> Far = Hanging(FVector(5000.0, 0.0, Top));
			VeyraSpringChain::Step(State, Far, Frame, Spring(), {}, Timing());
			for (int32 Joint = 0; Joint < Far.Num(); ++Joint)
			{
				ASSERT_THAT(IsTrue(State.Points[Joint].Equals(Far[Joint]), TEXT("respawned with it, not dragged across the map")));
			}
		}
	};
}

#endif
