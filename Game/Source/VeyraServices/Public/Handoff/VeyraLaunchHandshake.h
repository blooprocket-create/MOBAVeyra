// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/StringView.h"
#include "Containers/UnrealString.h"
#include "HAL/Platform.h"

/**
 * The game's lines of the launch handshake (ADR-010 §5). A launcher reads them on the game's standard
 * output: the game says when it is ready for its launch code, and whether it signed in. They are a
 * protocol shared with veyra-devlaunch and the launcher, fixed by Contracts/LaunchHandshake.json,
 * which the tests compare with these. No line ever carries a secret.
 */
namespace VeyraLaunchHandshake
{
	/** The game is ready to read its launch code on standard input. */
	inline constexpr const TCHAR* AwaitingLaunchCode = TEXT("veyra-handoff/1 awaiting-launch-code");

	/** The backend redeemed the launch code: the launcher's work is done. */
	inline constexpr const TCHAR* SignedIn = TEXT("veyra-handoff/1 signed-in");

	/** A failure line is this, a space, and one failure code. */
	inline constexpr const TCHAR* FailedPrefix = TEXT("veyra-handoff/1 failed");

	/** Why signing in failed, as the launcher is told. */
	enum class EFailure : uint8
	{
		/** No launch code arrived in time, or standard input closed or is not a pipe. */
		NoLaunchCode,
		/** Standard input held something that is not a launch code. */
		InvalidLaunchCode,
		/** The backend refused the code: expired, used, or for another build. */
		SignInRefused,
		/** The backend did not answer. */
		BackendUnreachable,
		/** The backend's answer was not understood. */
		BadAnswer,
		/** The game's own settings or command line are wrong. */
		Misconfigured,
	};

	/** The failure's code, as the contract spells it. */
	VEYRASERVICES_API const TCHAR* FailureCode(EFailure Failure);

	/** The whole failure line. */
	VEYRASERVICES_API FString FailedLine(EFailure Failure);

	/** Where the contract file is in the project folder, which exists only in editor builds. */
	VEYRASERVICES_API FString ContractPath();
}
