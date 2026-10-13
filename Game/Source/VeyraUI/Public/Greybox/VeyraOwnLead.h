// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Array.h"
#include "Math/Vector2D.h"
#include "Misc/CoreMiscDefines.h"
#include "Misc/Optional.h"

/** The values that shape the player's own body's lead along its path (ADR-077 §2), from the presentation settings. */
struct FVeyraOwnLeadShape
{
	/** How much faster than its speed the drawn body may move to take up its lead. */
	double CatchUpScale = 1.0;

	/** How long the server's body may stand while its path still runs on, as a stun or a stop to attack holds it, before the lead goes. */
	double StallSeconds = 0.0;

	/** How far from the path the server's body may stray, as other bodies push it, before the lead goes. */
	double OffPathDistance = 0.0;

	/** How long the lead takes to go, or to come back once the server's body walks on. */
	double BlendSeconds = 0.0;

	/** How long the step between two paths takes to close, as the server's path replaces the first guess. */
	double SwitchSeconds = 0.0;

	/** The farthest the body is ever drawn from where its smoothing has it. */
	double MaxDistance = 0.0;

	/** Within this of its path's end, the server's body has arrived and the lead goes. */
	double ArrivalTolerance = 0.0;

	/** The share of its speed below which the server's body counts as standing. */
	double StandingSpeedShare = 0.0;
};

/** What the player's machine knows this frame of its latest order and the body it moves (ADR-077 §2). */
struct FVeyraOwnLeadInputs
{
	/** This machine's real seconds now, when the order was given, and since the last frame. */
	double Now = 0.0;
	double GivenAt = 0.0;
	double DeltaSeconds = 0.0;

	/** Where the order leads: the point it named, or where the unit it chases is drawn. */
	FVector2D Goal = FVector2D::ZeroVector;

	/** An attack order's: how near its target the body stops, its reach. Unset for a move or an Attack Move. */
	TOptional<double> StopWithin;

	/** The server's path for this order once it has come, its points from start to end; empty until then. */
	TArray<FVector2D> ServerPath;

	/** Which of the server's paths that is, so a new one is taken up. */
	int32 ServerPathSerial = INDEX_NONE;

	/** Where the body's smoothing draws it from the server's updates, without any lead. */
	FVector2D Drawn = FVector2D::ZeroVector;

	/** How fast the server's body moves now, as its updates tell. */
	double ServerSpeed = 0.0;

	/** How fast the body walks, before any slow: its Movement Speed. */
	double Speed = 0.0;

	/**
	 * How many seconds the drawn body trails what the player's order has done by now: its smoothing's delay, half the round
	 * trip, and the wait for the server's next tick.
	 */
	double LeadSeconds = 0.0;

	/** Whether the body may be drawn ahead at all: alive, free to move, and moving under its own orders. */
	bool bMayLead = false;
};

/** The lead's state from frame to frame. */
struct FVeyraOwnLeadState
{
	/** The order it follows, by when it was given; and which of the server's paths it has taken up, if any. */
	double GivenAt = -1.0;
	int32 Serial = INDEX_NONE;

	/** The path the body is drawn along, and how far along it the body is drawn. */
	TArray<FVector2D> Path;
	double Arc = 0.0;

	/**
	 * How far along the path the order could have taken the body by now, walking at its speed since it was given: the body
	 * is never drawn past it, so it walks off at its own pace rather than racing ahead and waiting.
	 */
	double Ideal = 0.0;

	/** What is left of the step between two paths, closing over SwitchSeconds. */
	FVector2D Step = FVector2D::ZeroVector;

	/** How much of the lead shows, from 0 to 1. */
	double Weight = 0.0;

	/** How long the server's body has stood while its path still runs on. */
	double StandingFor = 0.0;

	/** The server's body's speed when it last walked, slows included. */
	double WalkingSpeed = 0.0;

	/** Where the body was last drawn, if it has been. */
	TOptional<FVector2D> Shown;
};

/** What the lead draws this frame. */
struct FVeyraOwnLead
{
	/** How far from where its smoothing has it the body is drawn. */
	FVector2D Offset = FVector2D::ZeroVector;

	/** The way the drawn body faces while it leads, in degrees of yaw; unset otherwise. */
	TOptional<double> Yaw;

	/** Whether the drawn body walks meanwhile, and how fast. */
	bool bWalks = false;
	double Speed = 0.0;
};

/**
 * The player's own body's lead along its path, as plain functions of what the player's machine knows, so tests check them
 * (ADR-077 §2). The drawn body walks off the moment its order is given and keeps ahead of the server's by what the order
 * will have done by now, along the order's path: a straight line to its goal at first, then the server's own. It never
 * passes the path's end, an attack's reach of its target, or its catch-up speed, and never steps back while it leads. It
 * lets the lead go when the server's body stands, strays from the path, arrives, or may not be led.
 */
namespace VeyraOwnLead
{
	/** One frame of the lead: State moves on from Inputs, shaped by Shape. */
	VEYRAUI_API FVeyraOwnLead Step(FVeyraOwnLeadState& State, const FVeyraOwnLeadInputs& Inputs, const FVeyraOwnLeadShape& Shape);

	/** Path's length. */
	VEYRAUI_API double LengthOf(const TArray<FVector2D>& Path);

	/** The point Arc along Path, clamped to its ends; the origin for an empty path. */
	VEYRAUI_API FVector2D PointAt(const TArray<FVector2D>& Path, double Arc);

	/** The way Path runs Arc along it, in degrees of yaw: its last segment's past its end. */
	VEYRAUI_API double YawAt(const TArray<FVector2D>& Path, double Arc);

	/** How far along Path its point nearest Point lies, and how far Point lies from it. */
	VEYRAUI_API double ArcNearest(const TArray<FVector2D>& Path, const FVector2D& Point, double* OutDistance = nullptr);

	/** How far along Path it first comes within Radius of Centre; Path's length if it never does. */
	VEYRAUI_API double ArcWithin(const TArray<FVector2D>& Path, const FVector2D& Centre, double Radius);
}
