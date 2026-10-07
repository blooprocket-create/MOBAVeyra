// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Math/Color.h"

class UVeyraGreyboxSettings;

/** Why a body is hidden from its enemies, as its veil shows it to its own side (ADR-068 §6). */
enum class EVeyraHiddenKind : uint8
{
	None,
	/** Within Dense Fog, which hides it from enemies outside the fog (Vision Bible §2). */
	DenseFog,
	/** Camouflaged: hidden but to True Sight and detection near it (Combat Bible §11). */
	Camouflage,
	/** Invisible: hidden but to True Sight (Combat Bible §11). */
	Invisible,
};

/** A hidden body's veil, as plain functions of what the client knows, so tests check them (ADR-068 §6). */
namespace VeyraHiddenBody
{
	/**
	 * Why a unit is hidden, as a viewer who bShows it sees (its own side, or a viewer on no side): the strongest of its
	 * reasons, Invisible over Camouflaged over within Dense Fog; None for a viewer on the other side, who sees no veil.
	 */
	VEYRAUI_API EVeyraHiddenKind KindOf(bool bShows, bool bInvisible, bool bCamouflaged, bool bInDenseFog);

	/** Veil, from 0 (solid) to 1 (veiled), stepped DeltaSeconds toward 1 while bHidden and toward 0 otherwise; a full change takes FadeSeconds. */
	VEYRAUI_API double StepVeil(double Veil, bool bHidden, double DeltaSeconds, double FadeSeconds);

	/** The colour of Kind's rim of light, as Settings give it; the fog's for None, which a fading veil keeps no longer. */
	VEYRAUI_API FLinearColor TintOf(EVeyraHiddenKind Kind, const UVeyraGreyboxSettings& Settings);
}
