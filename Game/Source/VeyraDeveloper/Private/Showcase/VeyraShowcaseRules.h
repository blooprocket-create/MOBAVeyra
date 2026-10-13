// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "VeyraAbilityTypes.h"

class UAbilitySystemComponent;

/** What the skill showcase's server and recorder agree on (ADR-074 §2, §3). */
namespace VeyraShowcase
{
	/** What a reply says of the reset it answers. */
	enum class EResetAnswer : uint8
	{
		/** It is not that reset's reply. */
		None,
		/** Both bodies stand at their marks, whole. */
		Set,
		/** A body could not be stood at its mark yet, as one held by another; ask again in a moment. */
		NotSet,
	};

	/** The server's reply to the reset asked for with Token: whether the stage is set, and what was done. */
	FString ResetReply(int32 Token, bool bSet, const FString& Done);

	/** What Reply says of the reset asked for with Token. */
	EResetAnswer ReadResetReply(const FString& Reply, int32 Token);

	/** What the recorder does when the server refuses a cast for a reason that does not pass with time. */
	enum class EAfterRefusal : uint8
	{
		/** The refusal passes with time: try again shortly. */
		Retry,
		/** The target is not ready for the ability, as one that must have been hurt a moment ago: hurt the dummy, then cast again. */
		Prime,
		/** Record the rest of the kit without it, and fail the recording. */
		Skip,
	};

	/** What to do after Rejection, once the dummy has been primed for this clip or not. */
	EAfterRefusal AfterRefusal(EVeyraCastRejection Rejection, bool bPrimed);

	/**
	 * Server: frees Unit's body of what would keep it from its mark or carry the last clip into the next, through Combat:
	 * a ride it is on ends as if its time ran out, a body it holds on to is let go, and every status it holds ends early,
	 * as a stance that plants it.
	 */
	void ReleaseHolds(UAbilitySystemComponent& Unit);
}
