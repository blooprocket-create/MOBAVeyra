// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "VeyraAbilityTypes.h"

class UAbilitySystemComponent;

/** What the skill showcase's server and recorder agree on (ADR-074 §2, §3). */
enum class EVeyraAttackPhase : uint8;

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
	 * Where a body stands after a reset: Stand from Centre on its own side of the line between the starts, that line
	 * turned YawDegrees about Centre. The camera looks along +X, so a turn of 90 stands the side starting at -X on the
	 * screen's left and the other on its right.
	 */
	FVector MarkOf(const FVector& Centre, const FVector& OwnStart, double Stand, double YawDegrees);

	/**
	 * Whether the basic attack's clip may start with its caster's attack in Phase (ADR-074 §3): only with none under way,
	 * so an order left from priming the dummy is never recorded as the clip's swing.
	 */
	bool MayStartAttackClip(EVeyraAttackPhase Phase);

	/** What follows the basic attack's clip (ADR-074 §3): the recording, after the rehearsal's; the encoding, after the recording's. */
	enum class EAfterAttack : uint8
	{
		Record,
		Flush,
	};
	EAfterAttack AfterAttack(bool bRehearsing);

	/**
	 * Server: frees Unit's body of what would keep it from its mark, through Combat: a ride it is on ends as if its time
	 * ran out, and a body it holds on to is let go.
	 */
	void ReleaseHolds(UAbilitySystemComponent& Unit);

	/**
	 * Whether the stage is clean for the next clip (ADR-074 §3): its caster holds no status, or it has waited
	 * StatusPatienceSeconds past the settle for them to end. A status ends through the system that owns it (a stance's
	 * mode, a lane, a slot it swapped), never by the reset, so the recorder waits for it.
	 */
	bool IsStageClean(bool bStatusesHeld, double SecondsSinceSet, double SettleSeconds, double StatusPatienceSeconds);

	/**
	 * Which slot to record next among what a skill opened (ADR-074 §3), before the stage is set again: of Held, each
	 * slot's ability in slot order, the first that is one of Opened (VeyraAbilityRules::AbilitiesOpenedBy) and not yet
	 * Recorded; none when no such slot is left. A ride's mounted actions are recorded while the ride lasts this way.
	 */
	TOptional<int32> NextOpenedSlot(TConstArrayView<FVeyraContentId> Held, TConstArrayView<FVeyraContentId> Opened,
		TConstArrayView<FVeyraContentId> Recorded);

	/** What the recorder does with an opened ability it chose (ADR-074 §3), given what its slot Held now. */
	enum class EOpenedHold : uint8
	{
		/** The slot still holds it: cast it. */
		Cast,
		/** Its slot went back to another ability before its clip began, as a ride that ran out: go on without it. */
		MoveOn,
		/** Its slot went back once its clip had begun: the clip is left out, never recorded as the slot's other ability. */
		Skip,
	};
	EOpenedHold OpenedHoldOf(const FVeyraContentId& Held, const FVeyraContentId& Opened, bool bClipOpen);
}
