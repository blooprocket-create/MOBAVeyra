// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Movement/VeyraUnitCollision.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace VeyraUnitCollision
{
ECollisionChannel ChannelOf(EVeyraTeam Team)
{
	switch (Team)
	{
	case EVeyraTeam::A:
		return SideA;
	case EVeyraTeam::B:
		return SideB;
	default:
		return ECC_Pawn;
	}
}

void ApplySide(UPrimitiveComponent& Body, EVeyraTeam Team)
{
	const ECollisionChannel Own = ChannelOf(Team);
	Body.SetCollisionObjectType(Own);
	SetResponseToUnits(Body, ECR_Block);
	// Allies pass through each other; a neutral unit blocks other neutral units, as every pawn did.
	if (Own != ECC_Pawn)
	{
		Body.SetCollisionResponseToChannel(Own, ECR_Ignore);
	}
}

void SetResponseToUnits(UPrimitiveComponent& Body, ECollisionResponse Response)
{
	for (const ECollisionChannel Channel : Channels)
	{
		Body.SetCollisionResponseToChannel(Channel, Response);
	}
}

FCollisionObjectQueryParams AllUnits()
{
	FCollisionObjectQueryParams Params;
	for (const ECollisionChannel Channel : Channels)
	{
		Params.AddObjectTypesToQuery(Channel);
	}
	return Params;
}

namespace
{
	// One avoidance group per side and role; the bits are the movement component's group flags.
	enum EAvoidanceBit : int32
	{
		SideAVanguards = 1 << 0,
		SideAFluxborn = 1 << 1,
		SideBVanguards = 1 << 2,
		SideBFluxborn = 1 << 3,
		Neutral = 1 << 4,
	};

	int32 VanguardsOf(EVeyraTeam Team)
	{
		return Team == EVeyraTeam::A ? SideAVanguards : Team == EVeyraTeam::B ? SideBVanguards : 0;
	}

	int32 FluxbornOf(EVeyraTeam Team)
	{
		return Team == EVeyraTeam::A ? SideAFluxborn : Team == EVeyraTeam::B ? SideBFluxborn : 0;
	}
}

FAvoidanceGroups AvoidanceGroupsOf(EVeyraTeam Team, EAvoidanceRole Role)
{
	const int32 Allies = VanguardsOf(Team) | FluxbornOf(Team);
	const EVeyraTeam Other = VeyraTeams::Opposing(Team);
	const int32 Enemies = VanguardsOf(Other) | FluxbornOf(Other);
	FAvoidanceGroups Groups;
	if (Team == EVeyraTeam::None)
	{
		// A unit on neither side steers around everyone.
		Groups.Group = Neutral;
		Groups.Avoid = SideAVanguards | SideAFluxborn | SideBVanguards | SideBFluxborn | Neutral;
		return Groups;
	}
	if (Role == EAvoidanceRole::Vanguard)
	{
		Groups.Group = VanguardsOf(Team);
		Groups.Avoid = Enemies | Neutral;
		Groups.Ignore = Allies;
	}
	else
	{
		Groups.Group = FluxbornOf(Team);
		Groups.Avoid = Allies | Enemies | Neutral;
	}
	return Groups;
}

void ApplyAvoidanceGroups(UCharacterMovementComponent& Movement, EVeyraTeam Team, EAvoidanceRole Role)
{
	const FAvoidanceGroups Groups = AvoidanceGroupsOf(Team, Role);
	Movement.SetAvoidanceGroupMask(Groups.Group);
	Movement.SetGroupsToAvoidMask(Groups.Avoid);
	Movement.SetGroupsToIgnoreMask(Groups.Ignore);
}
}
