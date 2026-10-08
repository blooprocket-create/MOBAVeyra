// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Map.h"
#include "Content/VeyraContentId.h"
#include "Misc/Optional.h"

struct FVeyraAbilitiesTuning;
struct FVeyraStatusEntry;

/** A self-buff's aura while its caster holds the buff (ADR-071 §3): how far it reaches, and until when in server time. */
struct FVeyraShownAura
{
	double Radius = 0.0;
	double EndsAt = 0.0;
};

/** A self-buff's end payload (ADR-071 §3): when it comes, in server time, and how far it reaches. */
struct FVeyraShownBurst
{
	double At = 0.0;
	double Radius = 0.0;
};

/**
 * What the presentation shows of each self-buff's aura and end payload, read from Abilities.json so nothing about
 * their reach is written twice (ADR-071 §3). A buff is known on every machine by the first of its statuses, which its
 * caster holds from the cast: its aura shows while that status lasts, and its payloads as they come after the cast.
 */
class VEYRAUI_API FVeyraKitPresentationIndex
{
public:
	static FVeyraKitPresentationIndex Build(const FVeyraAbilitiesTuning& Tuning);

	/** The auras the buff Entry marks still shows at Now; empty for a status that marks none. */
	TArray<FVeyraShownAura> AurasOf(const FVeyraStatusEntry& Entry, double Now) const;

	/** The payloads the buff Entry marks brings, timed from the status's start; empty for a status that marks none. */
	TArray<FVeyraShownBurst> BurstsOf(const FVeyraStatusEntry& Entry) const;

	bool IsEmpty() const { return ByStatus.IsEmpty(); }

private:
	struct FBuffShape
	{
		/** Each aura's reach and how long it lasts after the cast. */
		TArray<TPair<double, double>> Auras;

		/** Each payload's time after the cast and reach. */
		TArray<TPair<double, double>> Bursts;
	};
	TMap<FVeyraContentId, FBuffShape> ByStatus;
};

/** The kit presentation's timing (ADR-071). */
namespace VeyraKitPresentation
{
	/** How far a burst's ring has spread at Now: from 0 as it comes to 1 at its full reach Seconds later; unset outside that. */
	VEYRAUI_API TOptional<double> BurstShareAt(const FVeyraShownBurst& Burst, double Now, double Seconds);

	/**
	 * Where bead Bead of Count flows along a strand at Now, from 0 at its holder to 1 at its source, each crossing it
	 * in FlowSeconds, evenly spaced: a siphon drawn toward the unit that holds the other end (ADR-071 §2).
	 */
	VEYRAUI_API double BeadShareAt(int32 Bead, int32 Count, double Now, double FlowSeconds);
}
