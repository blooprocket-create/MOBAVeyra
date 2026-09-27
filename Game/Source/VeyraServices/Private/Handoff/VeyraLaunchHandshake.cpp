// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Handoff/VeyraLaunchHandshake.h"

#include "Misc/Paths.h"

namespace VeyraLaunchHandshake
{
const TCHAR* FailureCode(EFailure Failure)
{
	switch (Failure)
	{
	case EFailure::NoLaunchCode:
		return TEXT("no_launch_code");
	case EFailure::InvalidLaunchCode:
		return TEXT("invalid_launch_code");
	case EFailure::SignInRefused:
		return TEXT("sign_in_refused");
	case EFailure::BackendUnreachable:
		return TEXT("backend_unreachable");
	case EFailure::BadAnswer:
		return TEXT("bad_answer");
	case EFailure::Misconfigured:
		return TEXT("misconfigured");
	}
	checkNoEntry();
	return TEXT("");
}

FString FailedLine(EFailure Failure)
{
	return FString::Printf(TEXT("%s %s"), FailedPrefix, FailureCode(Failure));
}

FString ContractPath()
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("VeyraServices"), TEXT("Contracts"), TEXT("LaunchHandshake.json"));
}
}
