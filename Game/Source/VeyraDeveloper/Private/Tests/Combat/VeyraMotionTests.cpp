// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Movement/VeyraMotion.h"
#include "Movement/VeyraServerClock.h"
#include "Tuning/VeyraCombatTuning.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCombatTests
{
	// Veyra.Combat.Motion.*: a unit's motion and the clock it is read by (ADR-079 §1, §4). Every machine works out where a
	// body is from its motion alone, so these rules are what makes it walk, turn and stop where the server's own does.
	TEST_CLASS(Motion, "Veyra.Combat")
	{
		// Fixture values: a walk begun at StartTime from the origin, along +X then +Y, at Speed.
		static constexpr double StartTime = 10.0;
		static constexpr double Speed = 100.0;
		static constexpr double Leg = 200.0;
		static constexpr double Tolerance = 1e-3;

		static FVeyraMotion Walk()
		{
			FVeyraMotion Motion;
			Motion.Kind = EVeyraMotionKind::Walk;
			Motion.StartTime = StartTime;
			Motion.Start = FVector::ZeroVector;
			Motion.Corners = { FVector(Leg, 0.0, 0.0), FVector(Leg, Leg, 0.0) };
			Motion.Speed = static_cast<float>(Speed);
			return Motion;
		}

		static FVeyraMotionTuning ClockTuning()
		{
			// Fixture values: keep four exchanges, slew a tenth of a second a second, set at once past half a second.
			FVeyraMotionTuning Tuning;
			Tuning.ClockExchangesKept = 4;
			Tuning.ClockSlewRate = 0.1;
			Tuning.ClockSnapSeconds = 0.5;
			return Tuning;
		}

		TEST_METHOD(BeforeItBeginsItStandsWhereItBegan)
		{
			const FVeyraMotionSample Sample = VeyraMotion::Sample(Walk(), StartTime - 1.0);
			ASSERT_THAT(IsTrue(Sample.Location.Equals(FVector::ZeroVector, Tolerance)));
		}

		TEST_METHOD(ItWalksItsCornersAtItsSpeed)
		{
			const FVeyraMotion Motion = Walk();
			// Halfway along the first leg, then past the corner onto the second.
			const FVeyraMotionSample First = VeyraMotion::Sample(Motion, StartTime + 1.0);
			ASSERT_THAT(IsTrue(First.Location.Equals(FVector(Speed, 0.0, 0.0), Tolerance)));
			ASSERT_THAT(IsTrue(First.Velocity.Equals(FVector(Speed, 0.0, 0.0), Tolerance)));
			ASSERT_THAT(IsFalse(First.bEnded));
			const FVeyraMotionSample Turned = VeyraMotion::Sample(Motion, StartTime + 3.0);
			ASSERT_THAT(IsTrue(Turned.Location.Equals(FVector(Leg, Speed, 0.0), Tolerance), TEXT("it turns at the corner, never cutting it")));
			ASSERT_THAT(IsTrue(Turned.Velocity.Equals(FVector(0.0, Speed, 0.0), Tolerance)));
		}

		TEST_METHOD(ItStopsAtItsLastCorner)
		{
			const FVeyraMotion Motion = Walk();
			ASSERT_THAT(IsNear(VeyraMotion::Length(Motion), 2.0 * Leg, Tolerance));
			ASSERT_THAT(IsNear(VeyraMotion::EndTime(Motion), StartTime + 2.0 * Leg / Speed, Tolerance));
			const FVeyraMotionSample Sample = VeyraMotion::Sample(Motion, StartTime + 100.0);
			ASSERT_THAT(IsTrue(Sample.Location.Equals(FVector(Leg, Leg, 0.0), Tolerance)));
			ASSERT_THAT(IsTrue(Sample.Velocity.IsZero(), TEXT("never past it, never sliding on")));
			ASSERT_THAT(IsTrue(Sample.bEnded));
		}

		TEST_METHOD(AStandingBodyNeverMoves)
		{
			FVeyraMotion Motion = Walk();
			Motion.Kind = EVeyraMotionKind::Stand;
			const FVeyraMotionSample Sample = VeyraMotion::Sample(Motion, StartTime + 1.0);
			ASSERT_THAT(IsTrue(Sample.Location.Equals(FVector::ZeroVector, Tolerance) && Sample.Velocity.IsZero()));
			ASSERT_THAT(IsFalse(VeyraMotion::Walks(Motion)));
		}

		TEST_METHOD(AWalkIsToldNoFurtherThanItsHorizon)
		{
			// The path runs on past the horizon: what is told ends at the horizon, on the path.
			const TArray<FVector> Path = { FVector(Leg, 0.0, 0.0), FVector(Leg, Leg, 0.0) };
			const TArray<FVector_NetQuantize> Told = VeyraMotion::CornersWithin(FVector::ZeroVector, Path, Leg + Speed);
			ASSERT_THAT(AreEqual(Told.Num(), 2));
			ASSERT_THAT(IsTrue(FVector(Told[1]).Equals(FVector(Leg, Speed, 0.0), Tolerance)));
			// A corner on top of where it stands adds nothing.
			const TArray<FVector> Repeated = { FVector::ZeroVector, FVector(Leg, 0.0, 0.0) };
			ASSERT_THAT(AreEqual(VeyraMotion::CornersWithin(FVector::ZeroVector, Repeated, Leg * 2.0).Num(), 1));
		}

		TEST_METHOD(TheClockTrustsItsQuickestExchange)
		{
			const FVeyraMotionTuning Tuning = ClockTuning();
			FVeyraClockEstimate Clock;
			// A quick exchange: asked at 0, answered at 100, received at 0.02, so the server ran 100.01 - 0.02 ahead.
			Clock.AddExchange(0.0, 100.0, 0.02, Tuning);
			ASSERT_THAT(IsNear(Clock.GetOffset(), 99.99, Tolerance));
			// A slow one says otherwise; the quick one is still trusted, so the offset stays.
			Clock.AddExchange(1.0, 101.3, 1.4, Tuning);
			ASSERT_THAT(IsNear(Clock.GetOffset(), 99.99, Tolerance));
			ASSERT_THAT(IsNear(Clock.GetRoundTrip(), 0.02, Tolerance));
		}

		TEST_METHOD(TheClockMovesSlowlyUnlessItIsFarOff)
		{
			const FVeyraMotionTuning Tuning = ClockTuning();
			FVeyraClockEstimate Clock;
			Clock.AddExchange(0.0, 100.0, 0.02, Tuning);
			// A quicker exchange a little off is trusted now: the estimate slews toward it, never jumping.
			Clock.AddExchange(1.0, 101.05, 1.0, Tuning);
			ASSERT_THAT(IsNear(Clock.GetOffset(), 99.99, Tolerance));
			Clock.Advance(0.1, Tuning);
			ASSERT_THAT(IsNear(Clock.GetOffset(), 99.99 + 0.1 * Tuning.ClockSlewRate, Tolerance));
			// One far off is taken at once.
			Clock.AddExchange(2.0, 103.0, 2.0, Tuning);
			ASSERT_THAT(IsNear(Clock.GetOffset(), 101.0, Tolerance));
		}
	};
}

#endif
