// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Misc/Optional.h"
#include "Teams/VeyraTeam.h"

#include "VeyraMatchEnding.generated.h"

class AVeyraStructure;
class UWorld;

/** How the end of a match reads to one player while it watches the end (ADR-020 §1). */
UENUM()
enum class EVeyraEndingHeadline : uint8
{
	/** Its side destroyed the other side's Prime Well. */
	Victory,
	/** Its side's Prime Well fell. */
	Defeat,
	/** The match ended another way: a surrender, a remake or its host. The results say how. */
	MatchOver,
};

/** What a player sees as its match ends (ADR-020 §1): the fallen Prime Well, and a headline. */
namespace VeyraMatchEnding
{
	/**
	 * The Prime Well whose fall ended the match, as this machine sees it; null when the match ended
	 * another way, as in a practice match, where a Well decides nothing.
	 */
	VEYRAMATCH_API const AVeyraStructure* FindFallenPrimeWell(const UWorld& World);

	/** How the end reads to a player on Viewer's side, given the side whose Prime Well fell, if one did. */
	VEYRAMATCH_API EVeyraEndingHeadline Headline(const TOptional<EVeyraTeam>& FallenSide, EVeyraTeam Viewer);
}
