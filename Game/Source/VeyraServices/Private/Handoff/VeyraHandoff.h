// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "HAL/Platform.h"

/** The command line and exit status of the session handoff (ADR-005 L3, ADR-007). */
namespace VeyraHandoff
{
	/** -VeyraLaunchCode=stdin: a game reads its launch code from standard input and joins its match. */
	inline constexpr const TCHAR* LaunchCodeSwitch = TEXT("VeyraLaunchCode=");

	/** -VeyraAssignment=stdin: a match server reads its assignment from standard input. */
	inline constexpr const TCHAR* AssignmentSwitch = TEXT("VeyraAssignment=");

	/**
	 * -VeyraBackendUrl=<base URL>: the backend the launcher signed in to, which the game uses instead of its ini's
	 * (ADR-057 §5). An address, not a secret, so it may go on the command line.
	 */
	inline constexpr const TCHAR* BackendUrlSwitch = TEXT("VeyraBackendUrl=");

	/** The one channel either secret switch accepts: a secret never goes on a command line. */
	inline constexpr const TCHAR* StandardInput = TEXT("stdin");

	/** The engine's switch that runs each line of standard input as a console command and logs it. */
	inline constexpr const TCHAR* ConsoleFromStandardInputSwitch = TEXT("cmdstdin");

	/**
	 * The exit status requested when the handoff fails. Linux keeps it, so the backend sees a failed
	 * match server. A graceful Windows exit always returns 0, so for a game the "VeyraHandoff: FAIL"
	 * log line is the verdict, as for the smoke client.
	 */
	inline constexpr uint8 FailedExitCode = 1;
}
