// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Templates/Function.h"

class AVeyraPlayerController;

/**
 * How a player's developer command reaches the server (PROJECT_STRUCTURE.md "VeyraDeveloper").
 * VeyraDeveloper owns the Veyra.Dev.* commands and installs the handler that runs them; the match
 * knows only this route, so no production module depends on developer tooling. Shipping builds
 * install no handler, and their servers refuse every command before it gets here.
 */
namespace VeyraDeveloperCommandRoute
{
	/** Server: runs Command with Args for Requester, and returns what to tell them. */
	using FHandler = TFunction<FString(AVeyraPlayerController& Requester, const FString& Command, const TArray<FString>& Args)>;

	/** Installs Handler in place of any other; an empty one removes it. */
	VEYRAMATCH_API void SetHandler(FHandler Handler);

	/** Server: runs Command through the installed handler, or says that no handler is installed. */
	VEYRAMATCH_API FString Run(AVeyraPlayerController& Requester, const FString& Command, const TArray<FString>& Args);
}
