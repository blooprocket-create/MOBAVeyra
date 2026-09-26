// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Hash/Blake3.h"

/**
 * What a client must bring to join a match. A client reports the composite hash of its tuning in
 * its login options, and the server refuses any build whose tuning differs from its own
 * (ADR-006 §6).
 */
namespace VeyraJoinRules
{
	/** The login option that carries the client's composite tuning hash. */
	inline constexpr const TCHAR* TuningHashOption = TEXT("VeyraTuning");

	/** The login option for a composite tuning hash, as "VeyraTuning=<hex>". */
	VEYRAMATCH_API FString MakeTuningHashOption(const FBlake3Hash& CompositeHash);

	/**
	 * Why a client with these login options may not join a server whose composite tuning hash is
	 * ServerHash, or an empty string when it may.
	 */
	VEYRAMATCH_API FString CheckTuningHash(const FString& Options, const FBlake3Hash& ServerHash);

	/**
	 * Why a direct connection is refused, or an empty string. Development builds accept direct
	 * connections (ADR-005 step 2); Shipping builds refuse them until the match-join contract (M4).
	 */
	VEYRAMATCH_API FString CheckDirectConnect();
}
