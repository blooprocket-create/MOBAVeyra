// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Teams/VeyraTeam.h"

struct FVeyraFluxWellPresenceTuning;

/** Which side's presence drains an open Flux Well, and how fast. */
struct FVeyraPresenceDrain
{
	/** None when nobody drains it: nobody there, or equal sides contesting it. */
	EVeyraTeam Side = EVeyraTeam::None;
	double PerSecond = 0.0;
};

/**
 * The Flux Wells' rules as pure functions (Battleground Bible §6; ADR-014 §4, §9), so they are tested
 * without a world.
 */
namespace VeyraFluxWellRules
{
	/** How fast Count allied Vanguards drain a Well: one at the base rate, each further one adds, up to the cap. */
	VEYRAWORLD_API double DrainRate(int32 Count, const FVeyraFluxWellPresenceTuning& Presence);

	/**
	 * The presence drain with CountA and CountB Vanguards of each side at the Well: one side alone
	 * drains at its rate; contested, equal sides stall it and the larger drains at the contested
	 * fraction of the rate for its lead.
	 */
	VEYRAWORLD_API FVeyraPresenceDrain PresenceDrain(int32 CountA, int32 CountB, const FVeyraFluxWellPresenceTuning& Presence);

	/** Whether an open Well heals now: nobody has worked on it for IdleSeconds. */
	VEYRAWORLD_API bool Regenerates(double Now, double LastWorkedAt, double IdleSeconds);
}
