// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class FVeyraRiverShape;

/** The square of water the ripples are simulated over (ADR-076): its corner, in units, and how far it moved at its last step, in texels. */
struct FVeyraRippleWindow
{
	FVector2D Origin = FVector2D::ZeroVector;
	FIntPoint Shift = FIntPoint::ZeroValue;
	bool bPlaced = false;
};

/** The water's answer to the units wading it, as plain functions of what the client knows, so tests check them (ADR-076). */
namespace VeyraWaterRipples
{
	/**
	 * The window Size across centred near Focus, its corner snapped to whole texels of Resolution so the ripples it holds
	 * stay where they are on the water, and how far it moved from Previous in texels (none for a window not yet placed).
	 */
	VEYRAUI_API FVeyraRippleWindow Follow(const FVeyraRippleWindow& Previous, const FVector2D& Focus, double Size, int32 Resolution);

	/** How hard a unit moving at Speed stirs the water: Still standing, rising evenly to Moving at FullSpeed and holding there. */
	VEYRAUI_API double StrengthAt(double Speed, double Still, double Moving, double FullSpeed);

	/**
	 * A unit's stir as the ripple material takes it: its centre and radius as shares of the window, and its strength; none
	 * for a unit outside the window.
	 */
	VEYRAUI_API TOptional<FLinearColor> SourceIn(const FVeyraRippleWindow& Window, double Size, const FVector2D& At, double Radius, double Strength);

	/** Whether a unit with its feet at Feet wades the river: over its water and below its surface. */
	VEYRAUI_API bool Wades(const FVeyraRiverShape& River, double SurfaceZ, const FVector& Feet);

	/**
	 * Where a camera at Location looking along Direction looks on the water's plane at SurfaceZ: where its line of sight
	 * meets the plane, or right under it when it looks no lower than level.
	 */
	VEYRAUI_API FVector2D FocusOf(const FVector& Location, const FVector& Direction, double SurfaceZ);
}
