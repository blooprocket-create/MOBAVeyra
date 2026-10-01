// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Tuning/VeyraVisionTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVisionTests
{
	// Veyra.Vision.VisionTuning.*: the committed Vision.json loads, binds and passes the domain's checks.
	TEST_CLASS(VisionTuning, "Veyra.Vision")
	{
		TEST_METHOD(TheCommittedFileLoadsAndBinds)
		{
			const FVeyraVisionTuning& Tuning = UVeyraVisionTuningSubsystem::Get();
			ASSERT_THAT(IsTrue(Tuning.Update.UpdateSeconds > 0.0));
			ASSERT_THAT(IsTrue(Tuning.Sight.Vanguard > 0.0 && Tuning.Sight.Fluxborn > 0.0 && Tuning.Sight.Structure > 0.0 && Tuning.Sight.Ward > 0.0
				&& Tuning.Sight.Companion > 0.0));
			// Canon: a Vanguard carries three ward charges (Vision Bible §4).
			constexpr int32 CanonWardCharges = 3;
			ASSERT_THAT(IsTrue(Tuning.WardCharges.Max == CanonWardCharges && Tuning.WardCharges.Provenance == EVeyraTuningProvenance::Canon));
			ASSERT_THAT(IsTrue(Tuning.PersistentWard.HitsToDestroy >= 1 && Tuning.PersistentWard.LifetimeSeconds > 0.0 && Tuning.PersistentWard.RechargeSeconds > 0.0));
			ASSERT_THAT(IsTrue(VeyraVision::Validate(Tuning).IsEmpty()));
		}

		TEST_METHOD(AWardsBodyIsNeverShorterThanItIsWide)
		{
			FVeyraVisionTuning Tuning = UVeyraVisionTuningSubsystem::Get();
			Tuning.PersistentWard.BodyHalfHeight = Tuning.PersistentWard.BodyRadius / 2.0;
			ASSERT_THAT(IsTrue(VeyraVision::Validate(Tuning).Num() == 1));
		}
	};
}

#endif
