// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Cues/VeyraCombatCues.h"
#include "Math/Vector.h"

/** How a fight's blows feel on this machine, as plain functions, so tests check them (ADR-068 §4). Presentation only. */
namespace VeyraHitFeel
{
	/**
	 * How hard a cue of Kind kicks the camera of the player whose own Vanguard it befell, in units, times Scale (the
	 * player's Screen Shake): a death, DeathAmplitude; a hit that took at least HeavyShare of the Vanguard's MaxHealth
	 * (Amount, its Health and shields lost), HitAmplitude; anything else, none.
	 */
	VEYRAUI_API double AmplitudeOf(EVeyraCombatCueKind Kind, double Amount, double MaxHealth, double HeavyShare, double HitAmplitude, double DeathAmplitude,
		double Scale);

	/**
	 * The camera's offset At seconds into a kick of Amplitude lasting Seconds: a shudder, round at Frequency turns a second
	 * across the view (Y, its right) and up it (Z), fading from Amplitude to nothing; none before it starts or once it ends.
	 */
	VEYRAUI_API FVector OffsetAt(double Amplitude, double At, double Seconds, double Frequency);

	/** Whether a kick of Amplitude that began At seconds ago and lasts Seconds still moves the camera more than a fresh one of Fresh would. */
	VEYRAUI_API bool Outshakes(double Amplitude, double At, double Seconds, double Fresh);
}
