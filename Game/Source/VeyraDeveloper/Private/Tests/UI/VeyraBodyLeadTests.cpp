// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Greybox/VeyraBodyLead.h"

namespace VeyraBodyLeadTests
{
	// Veyra.UI.BodyLead.*: the player's own body leads its latest order before the server's movement reaches it (ADR-067 §2).
	TEST_CLASS(BodyLead, "Veyra.UI")
	{
		// Fixture values: the lead's window and angle, a body's reach and speed, and an order's point ahead and to the left.
		static constexpr double LeadSeconds = 0.35;
		static constexpr double AlignDegrees = 30.0;
		static constexpr double Reach = 40.0;
		static constexpr double Speed = 330.0;
		static constexpr double Fresh = 0.05;

		/** The lead of a body at the origin toward Where, ordered Age seconds ago, moving at Velocity. */
		static FVeyraBodyLead Lead(double Age, const FVector& Velocity, bool bRun = true, const FVector& Where = FVector(0.0, 500.0, 0.0))
		{
			return VeyraBodyLead::For(Where, Age, FVector::ZeroVector, Velocity, Speed, bRun, LeadSeconds, AlignDegrees, Reach);
		}

		TEST_METHOD(AFreshOrderTurnsTheStillBodyTowardItsPointAndRuns)
		{
			const FVeyraBodyLead Fast = Lead(Fresh, FVector::ZeroVector);
			ASSERT_THAT(IsTrue(Fast.bLeads));
			ASSERT_THAT(IsNear(Fast.Yaw, 90.0, 1e-6));
			ASSERT_THAT(IsNear(Fast.GroundSpeed, Speed, 1e-6));
			// Moving the other way, as before a turn, it leads all the same.
			ASSERT_THAT(IsTrue(Lead(Fresh, FVector(0.0, -Speed, 0.0)).bLeads));
		}

		TEST_METHOD(TheServersMovementTakesOverOnceItHeadsThatWay)
		{
			const FVector Toward(0.0, Speed, 0.0);
			ASSERT_THAT(IsFalse(Lead(Fresh, Toward).bLeads));
			// Within the angle, still the server's; beyond it, still the lead's.
			const FVector Within = Toward.RotateAngleAxis(AlignDegrees - 1.0, FVector::UpVector);
			const FVector Beyond = Toward.RotateAngleAxis(AlignDegrees + 1.0, FVector::UpVector);
			ASSERT_THAT(IsFalse(Lead(Fresh, Within).bLeads));
			ASSERT_THAT(IsTrue(Lead(Fresh, Beyond).bLeads));
		}

		TEST_METHOD(ALeadEndsWithItsWindowAndNeverStartsWithinReach)
		{
			ASSERT_THAT(IsFalse(Lead(LeadSeconds, FVector::ZeroVector).bLeads, TEXT("at most its window")));
			ASSERT_THAT(IsFalse(Lead(-Fresh, FVector::ZeroVector).bLeads, TEXT("never before the order")));
			ASSERT_THAT(IsFalse(Lead(Fresh, FVector::ZeroVector, true, FVector(Reach / 2.0, 0.0, 0.0)).bLeads, TEXT("a point within its reach")));
		}

		TEST_METHOD(AnAttackOrderTurnsTheBodyWithoutRunning)
		{
			const FVeyraBodyLead Turn = Lead(Fresh, FVector::ZeroVector, /*bRun*/ false);
			ASSERT_THAT(IsTrue(Turn.bLeads && Turn.GroundSpeed == 0.0));
		}

		TEST_METHOD(AFacingLeadStandsWhileARunningLeadRunsAtLeastItsSpeed)
		{
			// Running when the click came: an attack's target within reach is faced standing, not run at for a round trip.
			const FVector Away(0.0, -Speed, 0.0);
			ASSERT_THAT(IsNear(VeyraBodyLead::GroundSpeedOf(Lead(Fresh, Away, /*bRun*/ false), Speed), 0.0, 1e-6));
			ASSERT_THAT(IsNear(VeyraBodyLead::GroundSpeedOf(Lead(Fresh, FVector::ZeroVector), 0.0), Speed, 1e-6));
			ASSERT_THAT(IsNear(VeyraBodyLead::GroundSpeedOf(FVeyraBodyLead(), Speed / 2.0), Speed / 2.0, 1e-6, TEXT("no lead, the server's")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI
