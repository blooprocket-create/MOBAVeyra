// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Misc/Optional.h"

struct FVeyraMasteryEmoteTuning;

/**
 * The mastery emote's rule (ADR-045 §9; Account, Collection & Mastery Bible §5.2): a player with Mastery to show
 * may show it, at most once per cooldown. It has no gameplay effect. Times are world seconds.
 */
namespace VeyraMasteryEmote
{
	/** Until when the emote shows if a player with MasteryLevel asks at Now, when it may next show at NextAllowedAt; unset when refused. */
	VEYRAMATCH_API TOptional<double> Show(int32 MasteryLevel, double Now, double NextAllowedAt, const FVeyraMasteryEmoteTuning& Tuning);
}
