// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Misc/AutomationTest.h"

namespace VeyraNetTests
{
	/**
	 * Iris warnings every networked test can meet that report nothing wrong with Veyra:
	 * - Every new PlayerController's camera manager sets the controller as its view target, and on
	 *   the server that sends ClientSetViewTarget before Iris has registered the controller, so Iris
	 *   refuses the RPC with a warning. The client's own camera manager makes the same assignment, so
	 *   nothing is lost. The server's view of each player is set by Veyra code, not by this RPC.
	 * - Iris numbers each replication system's object groups with an 8-bit epoch counted across the
	 *   whole process, and warns as it wraps. Each networked test starts about three replication
	 *   systems, so one test in every eighty-odd meets it; the groups it hands out stay distinct.
	 */
	inline void IgnoreKnownIrisWarnings(FAutomationTestBase& TestRunner)
	{
		constexpr int32 IgnoreAnyNumber = -1;
		TestRunner.AddExpectedMessagePlain(TEXT("SendRPC ClientSetViewTarget for"), ELogVerbosity::Warning,
			EAutomationExpectedMessageFlags::Contains, IgnoreAnyNumber);
		TestRunner.AddExpectedMessagePlain(TEXT("FNetObjectGroups::Epoch wraparound detected."), ELogVerbosity::Warning,
			EAutomationExpectedMessageFlags::Contains, IgnoreAnyNumber);
	}
}
