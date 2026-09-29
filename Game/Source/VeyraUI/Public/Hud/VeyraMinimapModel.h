// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Math/Vector.h"
#include "Math/Vector2D.h"
#include "Misc/Optional.h"
#include "Teams/VeyraTeam.h"

class UWorld;

/** Where the minimap sits on the screen and what it shows of the world (Settings Bible §3.2; ADR-020 §2). */
struct FVeyraMinimapFrame
{
	/** The map's top-left corner, in screen pixels. */
	FVector2D Origin = FVector2D::ZeroVector;

	/** Its side, in pixels: the map is square, as the battleground's floor is. */
	double Size = 0.0;

	/** How far the floor reaches from the map's centre on each axis, in units. */
	double HalfExtent = 0.0;
};

/** What a dot on the minimap stands for. */
enum class EVeyraMinimapDot : uint8
{
	Vanguard,
	Fluxborn,
	Wildlife,
	Ward,
	Structure,
};

/** How the viewer's side stands to a dot. */
enum class EVeyraMinimapSide : uint8
{
	Own,
	Ally,
	Enemy,
	Neutral,
};

struct FVeyraMinimapDot
{
	FVector2D Position = FVector2D::ZeroVector;
	EVeyraMinimapDot Kind = EVeyraMinimapDot::Fluxborn;
	EVeyraMinimapSide Side = EVeyraMinimapSide::Neutral;
};

/** A lane as a line of map points. */
struct FVeyraMinimapLane
{
	TArray<FVector2D> Points;
};

/** A presence ping's circle, as the map draws it. */
struct FVeyraMinimapPing
{
	FVector2D Centre = FVector2D::ZeroVector;
	double Radius = 0.0;
	double Fade = 0.0;
};

/** Everything the minimap draws this frame, in screen pixels. */
struct FVeyraMinimapView
{
	FVeyraMinimapFrame Frame;
	TArray<FVeyraMinimapLane> Lanes;
	TArray<FVeyraMinimapDot> Dots;
	TArray<FVeyraMinimapPing> Pings;
	/** The camera's focus, where the player is looking. */
	TOptional<FVector2D> Focus;
};

/**
 * The minimap's pure geometry and what it shows (ADR-020 §2). The map is oriented as the camera looks:
 * the world's +X is up and +Y is right. It shows what the viewer's client has: the fog gate already
 * keeps back every unit its side may not see (ADR-016), so nothing here decides visibility.
 */
namespace VeyraMinimap
{
	/** The frame in the bottom-right of a Viewport-sized screen, Size pixels square, Margin in from the edges. */
	VEYRAUI_API FVeyraMinimapFrame FrameFor(const FVector2D& Viewport, double Size, double Margin, double HalfExtent);

	/** Where World falls on the map, in screen pixels. */
	VEYRAUI_API FVector2D ToMap(const FVeyraMinimapFrame& Frame, const FVector& World);

	/** The ground point a screen pixel on the map stands for; nothing outside the map. */
	VEYRAUI_API TOptional<FVector> ToWorld(const FVeyraMinimapFrame& Frame, const FVector2D& Screen);

	/** How Viewer's side stands to Team; the viewer's own unit is Own. */
	VEYRAUI_API EVeyraMinimapSide SideOf(EVeyraTeam Viewer, EVeyraTeam Team, bool bOwn);

	/** Gathers what World's client has for the minimap, for a viewer on Viewer's side whose Vanguard is Own. */
	VEYRAUI_API FVeyraMinimapView Describe(const UWorld& World, const FVeyraMinimapFrame& Frame, EVeyraTeam Viewer, const AActor* Own,
		const TOptional<FVector>& Focus, double ServerNow);
}
