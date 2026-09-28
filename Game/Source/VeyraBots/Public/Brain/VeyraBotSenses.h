// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Battleground/VeyraBattlegroundTypes.h"
#include "Brain/VeyraBotView.h"

class AVeyraPlayerState;

/**
 * What a bot knows (ADR-013 §4): read from the world into plain data at each decision. It sees what
 * is near it, as a player sees their screen; fog arrives with Vision. Server only.
 */
namespace VeyraBotSenses
{
	/** What Bot knows now, playing Lane. */
	VEYRABOTS_API FVeyraBotView Sense(const AVeyraPlayerState& Bot, EVeyraLane Lane, const FVeyraBotsTuning& Tuning);
}
