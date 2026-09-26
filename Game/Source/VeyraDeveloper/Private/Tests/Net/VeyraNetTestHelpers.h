// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Misc/AutomationTest.h"

namespace VeyraNetTests
{
	// Every new PlayerController's camera manager sets the controller as its view target, and on
	// the server that sends ClientSetViewTarget before Iris has registered the controller, so Iris
	// refuses the RPC with a warning. The client's own camera manager makes the same assignment, so
	// nothing is lost. The server's view of each player is set by Veyra code, not by this RPC.
	inline void IgnoreLoginViewTargetRpc(FAutomationTestBase& TestRunner)
	{
		constexpr int32 IgnoreAnyNumber = -1;
		TestRunner.AddExpectedMessagePlain(TEXT("SendRPC ClientSetViewTarget for"), ELogVerbosity::Warning,
			EAutomationExpectedMessageFlags::Contains, IgnoreAnyNumber);
	}
}
