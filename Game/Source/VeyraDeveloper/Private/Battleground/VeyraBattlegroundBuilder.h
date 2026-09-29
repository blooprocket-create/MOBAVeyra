// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Greybox/VeyraGreyboxLayout.h"
#include "Tuning/VeyraWorldTuning.h"

class UWorld;

/**
 * Builds the battleground's geometry from World.json's layout (ADR-011 §12). The map commandlet
 * saves it as L_Battleground; the network tests build it in each play session's worlds at runtime.
 * The floor's thickness, the navigation bounds' height and the sun come from the grey box's own
 * build settings, Greybox.json, which both generated maps share.
 */
namespace VeyraBattlegroundBuilder
{
	/** The grey box's build settings with the battleground's floor, to reuse VeyraGreybox's builders. */
	FVeyraGreyboxLayout AsGreybox(const FVeyraBattlegroundLayout& Layout, const FVeyraGreyboxLayout& Greybox);

	/** Each team's start at its fountain, facing the map's centre. Only the server uses them. */
	void SpawnTeamStarts(UWorld& World, const FVeyraBattlegroundLayout& Layout);

	/** The floor, navigation bounds for a world already playing, and the team starts: what a network test needs. */
	void SpawnRuntimeBattleground(UWorld& World, const FVeyraBattlegroundLayout& Layout, const FVeyraGreyboxLayout& Greybox, bool bServer);
}
