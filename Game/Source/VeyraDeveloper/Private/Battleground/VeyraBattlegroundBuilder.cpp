// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Battleground/VeyraBattlegroundBuilder.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Layout/VeyraLayout.h"
#include "VeyraTeamStart.h"

namespace VeyraBattlegroundBuilder
{
FVeyraGreyboxLayout AsGreybox(const FVeyraBattlegroundLayout& Layout, const FVeyraGreyboxLayout& Greybox)
{
	FVeyraGreyboxLayout Floor = Greybox;
	Floor.Floor.LengthX = Layout.HalfExtent * 2.0;
	Floor.Floor.WidthY = Layout.HalfExtent * 2.0;
	return Floor;
}

void SpawnTeamStarts(UWorld& World, const FVeyraBattlegroundLayout& Layout)
{
	for (const EVeyraTeam Team : { EVeyraTeam::A, EVeyraTeam::B })
	{
		AVeyraTeamStart* Start = World.SpawnActorDeferred<AVeyraTeamStart>(AVeyraTeamStart::StaticClass(), FTransform::Identity);
		Start->SetVeyraTeam(Team);
		// Stand the start's capsule on the floor at the fountain, facing the centre of the map.
		const FVector2D Fountain = VeyraLayout::Fountain(Layout, Team);
		const FVector Location(Fountain.X, Fountain.Y, Start->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		const FVector Facing = FVector(-Fountain.X, -Fountain.Y, 0.0).GetSafeNormal();
		Start->FinishSpawning(FTransform(Facing.Rotation(), Location));
	}
}

void SpawnRuntimeBattleground(UWorld& World, const FVeyraBattlegroundLayout& Layout, const FVeyraGreyboxLayout& Greybox, bool bServer)
{
	const FVeyraGreyboxLayout Floor = AsGreybox(Layout, Greybox);
	VeyraGreybox::SpawnFloor(World, Floor, EComponentMobility::Movable);
	if (bServer)
	{
		SpawnTeamStarts(World, Layout);
		VeyraGreybox::SpawnRuntimeNavigationBounds(World, Floor);
	}
}
}
