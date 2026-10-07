// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Math/Vector.h"

/** How the player's own drawn body leads its latest order now (ADR-067 §2). */
struct FVeyraBodyLead
{
	/** Whether it leads at all. */
	bool bLeads = false;

	/** The facing it turns toward, in degrees of yaw. */
	double Yaw = 0.0;

	/** The ground speed its animation runs at meanwhile; 0 when it only turns, as toward an attack's target. */
	double GroundSpeed = 0.0;
};

/** The player's own body's lead, as plain functions of what the client knows, so tests check them (ADR-067 §2). */
namespace VeyraBodyLead
{
	/**
	 * The lead toward an order's Point given OrderAge seconds ago, for a body at BodyLocation moving at Velocity: none once the
	 * order is LeadSeconds old, while the point lies within ArrivalRadius, or once the body's own movement heads within
	 * AlignDegrees of the point, as the server's movement takes over. With bRun it runs at MoveSpeed meanwhile.
	 */
	VEYRAUI_API FVeyraBodyLead For(const FVector& Point, double OrderAge, const FVector& BodyLocation, const FVector& Velocity, double MoveSpeed, bool bRun,
		double LeadSeconds, double AlignDegrees, double ArrivalRadius);

	/**
	 * The ground speed the body's animation runs at, given the server's movement's ServerSpeed: a lead that runs, at least its
	 * own; a lead that only faces, as toward an attack's target within reach, none, whatever the server last moved it at; no
	 * lead, the server's.
	 */
	VEYRAUI_API double GroundSpeedOf(const FVeyraBodyLead& Lead, double ServerSpeed);
}
