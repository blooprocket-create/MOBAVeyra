// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/CoreNetTypes.h"

class AActor;
class UActorComponent;

/**
 * A participant's data behind the fog (ADR-006 §5, ADR-016 §3). A participant's PlayerState reaches
 * every machine, but its Health, statuses, combat state, cast and attack need not: they replicate with
 * COND_NetGroup, in a group of their own, which reaches the participant's own connection, the replay,
 * and the connections Vision puts in the group — its teammates and whoever sees its Vanguard. An
 * observer who loses sight keeps the value it last saw.
 */
namespace VeyraParticipantData
{
	/** Whether Component's owner is a participant, whose data the fog hides: a PlayerState. */
	VEYRACOMBAT_API bool IsParticipantData(const UActorComponent& Component);

	/** COND_NetGroup for a participant's data, Otherwise for anything else. */
	VEYRACOMBAT_API ELifetimeCondition ConditionFor(const UActorComponent& Component, ELifetimeCondition Otherwise);

	/** The net condition group of Participant's data, which Vision fills with connections. */
	VEYRACOMBAT_API FName GroupOf(const AActor& Participant);

	/**
	 * Server: puts SubObject, which replicates with COND_NetGroup, in Participant's group, its owner's
	 * and the replay's. Iris sends such a subobject only through a group it knows the subobject is in,
	 * and learns that only once the subobject replicates: gate before the first send, then ApplyGates
	 * as the participant starts replicating. Does nothing on a client or without networking.
	 */
	VEYRACOMBAT_API void Gate(UObject& SubObject, const AActor& Participant);

	/**
	 * Server: tells Iris the groups of Participant's gated components and attribute sets, now that
	 * they replicate; until then they reach nobody, not even the participant's own client.
	 */
	VEYRACOMBAT_API void ApplyGates(const AActor& Participant);
}
