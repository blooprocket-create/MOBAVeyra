// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/StaticArray.h"
#include "Math/Box2D.h"
#include "Math/Vector2D.h"

struct FVeyraWallRequest;

/**
 * A wall's footprint on the ground (ADR-032 §4; ADR-042 §1): a box Length long across the way it faces
 * and Thickness deep along it. Pure geometry, for what must reason about terrain without the physics
 * scene: the layout's validation and sight (ADR-042 §3).
 */
struct VEYRACOMBAT_API FVeyraTerrainBox
{
	FVector2D Centre = FVector2D::ZeroVector;

	/** The way it faces, a unit vector: its thickness runs along it and its length across it. */
	FVector2D Facing = FVector2D(1.0, 0.0);

	double Length = 0.0;
	double Thickness = 0.0;

	/** The footprint of a raised wall. */
	static FVeyraTerrainBox Of(const FVeyraWallRequest& Request);

	/** Its four corners, round its outline. */
	TStaticArray<FVector2D, 4> Corners() const;

	/** The axis-aligned box that holds it. */
	FBox2D Bounds() const;

	/** How far Point lies from the box; 0 on or inside it. */
	double DistanceTo(const FVector2D& Point) const;

	/** Whether the segment From–To passes through the box, touching its edge included. */
	bool Crosses(const FVector2D& From, const FVector2D& To) const;

	/** How near the segment From–To comes to the box; 0 where it crosses it. */
	double DistanceToSegment(const FVector2D& From, const FVector2D& To) const;
};
