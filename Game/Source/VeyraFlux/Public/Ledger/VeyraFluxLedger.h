// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Array.h"
#include "Misc/Optional.h"
#include "Tuning/VeyraFluxTuning.h"

/** A temporary grant still counting (Battleground Bible §10). */
struct FVeyraTemporaryFlux
{
	double Amount = 0.0;

	/** When it falls away, in the server's world time. */
	double ExpiresAt = 0.0;
};

/** How much stronger a team's Fluxborn are for its active Team Flux (Battleground Bible §4). */
struct FVeyraFluxbornStrength
{
	/** Max Health multiplier: 1 is the Fluxborn's own. */
	double HealthMultiplier = 1.0;

	/** Damage multiplier: 1 is the Fluxborn's own. */
	double DamageMultiplier = 1.0;

	bool operator==(const FVeyraFluxbornStrength& Other) const = default;
};

/**
 * One team's Team Flux (PROJECT_STRUCTURE.md, VeyraFlux): the permanent total, and the temporary
 * grants each on its own timer. No world: the caller supplies the time.
 */
struct VEYRAFLUX_API FVeyraFluxLedger
{
	double Permanent = 0.0;
	TArray<FVeyraTemporaryFlux> Temporary;

	/** Adds Grant at world time Now. A temporary grant never refreshes an older one (§10). */
	void Add(const FVeyraFluxGrantTuning& Grant, double Now);

	/** Drops the temporary grants that have expired by Now. Returns whether any did. */
	bool Expire(double Now);

	/** Permanent Flux plus the temporary grants still counting at Now. */
	double Active(double Now) const;

	/** When the next temporary grant falls away; none when no temporary grant counts. */
	TOptional<double> NextExpiry() const;
};

namespace VeyraFlux
{
	/** The strength Active Team Flux gives a team's Fluxborn: each full step adds its fractions (§4). */
	VEYRAFLUX_API FVeyraFluxbornStrength StrengthFor(double Active, const FVeyraFluxbornScalingTuning& Scaling);
}
