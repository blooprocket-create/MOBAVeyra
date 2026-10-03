// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Movement/VeyraUnitCollision.h"

#include "Components/PrimitiveComponent.h"

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
}
