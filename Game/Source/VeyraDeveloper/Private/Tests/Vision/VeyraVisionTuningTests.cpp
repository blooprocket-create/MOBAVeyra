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
			ASSERT_THAT(IsTrue(Tuning.Sight.Vanguard > 0.0 && Tuning.Sight.Fluxborn > 0.0 && Tuning.Sight.Structure > 0.0));
			ASSERT_THAT(IsTrue(VeyraVision::Validate(Tuning).IsEmpty()));
		}
	};
}

#endif
