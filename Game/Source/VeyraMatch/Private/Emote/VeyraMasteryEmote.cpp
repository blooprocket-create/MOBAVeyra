// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Emote/VeyraMasteryEmote.h"

#include "Tuning/VeyraMatchTuning.h"

TOptional<double> VeyraMasteryEmote::Show(int32 MasteryLevel, double Now, double NextAllowedAt, const FVeyraMasteryEmoteTuning& Tuning)
{
	// No Mastery to show (a bot, or a match the backend gave no progression), or within the cooldown.
	if (MasteryLevel <= 0 || Now < NextAllowedAt)
	{
		return {};
	}
	return Now + Tuning.Seconds;
}
