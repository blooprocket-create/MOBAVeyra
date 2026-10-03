// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"
#include "Teams/VeyraTeam.h"

class UCharacterMovementComponent;
class UPrimitiveComponent;

/**
 * Unit collision by side (Combat Bible §24; ADR-062 §1). Each side's units sit on their own collision
 * object channel and block the other side's and neutral units; allies pass through each other. Neutral
 * units keep the pawn channel and block both sides. Structures, terrain and anything else that blocks
 * pawns block both sides' channels by default (DefaultEngine.ini).
 */
namespace VeyraUnitCollision
{
	/** Side A's and side B's unit channels (DefaultEngine.ini: VeyraSideA, VeyraSideB). */
	inline constexpr ECollisionChannel SideA = ECC_GameTraceChannel2;
	inline constexpr ECollisionChannel SideB = ECC_GameTraceChannel3;

	/** Every channel a unit's body sits on: neutral units' pawn channel and each side's. */
	inline constexpr ECollisionChannel Channels[] = { ECC_Pawn, SideA, SideB };

	/** A side's unit channel; the pawn channel for a neutral unit. */
	VEYRACOMBAT_API ECollisionChannel ChannelOf(EVeyraTeam Team);

	/** Puts Body on Team's channel, passing through its allies and blocking enemy and neutral units. */
	VEYRACOMBAT_API void ApplySide(UPrimitiveComponent& Body, EVeyraTeam Team);

	/** Gives Body one response to every unit, whatever its side: Block for a ward or a marker, Ignore for a ghost. */
	VEYRACOMBAT_API void SetResponseToUnits(UPrimitiveComponent& Body, ECollisionResponse Response);

	/** Object query parameters that find every unit, whatever its side. */
	VEYRACOMBAT_API FCollisionObjectQueryParams AllUnits();

	/** What a unit the server moves is, for the avoidance groups it steers by (ADR-062 §3). */
	enum class EAvoidanceRole : uint8
	{
		Vanguard,
		Fluxborn,
	};

	/** A unit's avoidance group, the groups it steers around, and the groups it never steers for. */
	struct FAvoidanceGroups
	{
		int32 Group = 0;
		int32 Avoid = 0;
		int32 Ignore = 0;
	};

	/**
	 * Who steers around whom (ADR-062 §3). A Vanguard steers around enemy and neutral units and walks
	 * through its allies, which it passes anyway. A Fluxborn steers around everyone, its own side's
	 * Vanguards included, so an allied wave makes way for them (Combat Bible §24).
	 */
	VEYRACOMBAT_API FAvoidanceGroups AvoidanceGroupsOf(EVeyraTeam Team, EAvoidanceRole Role);

	/** Sets Movement's avoidance groups from AvoidanceGroupsOf. */
	VEYRACOMBAT_API void ApplyAvoidanceGroups(UCharacterMovementComponent& Movement, EVeyraTeam Team, EAvoidanceRole Role);
}
