// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Terrain/VeyraTerrainMass.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "NavAreas/NavArea_Null.h"
#include "Terrain/VeyraRuntimeTerrain.h"

AVeyraTerrainMass::AVeyraTerrainMass()
{
	PrimaryActorTick.bCanEverTick = false;
	// Every machine builds its own from the layout they share.
	bReplicates = false;
	SetReplicatingMovement(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AVeyraTerrainMass::Build(TConstArrayView<FVeyraWallRequest> Volumes)
{
	UWorld* World = GetWorld();
	if (!World || !Boxes.IsEmpty())
	{
		return;
	}
	// Only the server navigates; its paths go round every box.
	const bool bNavigates = World->GetNetMode() != NM_Client;
	for (const FVeyraWallRequest& Volume : Volumes)
	{
		const FVector Facing = Volume.Facing.GetSafeNormal2D();
		if (Facing.IsNearlyZero() || !(Volume.Length > 0.0) || !(Volume.Thickness > 0.0) || !(Volume.HalfHeight > 0.0))
		{
			continue;
		}
		// World-static, as an ability's wall is: movement, forced moves and line projectiles meet it, never the cursor's
		// trace, the camera or the markers' channel.
		UBoxComponent* Box = NewObject<UBoxComponent>(this, NAME_None, RF_Transient);
		Box->SetCollisionObjectType(ECC_WorldStatic);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		Box->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		Box->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
		Box->SetGenerateOverlapEvents(false);
		// It cuts the navigation, rather than costing it: no path crosses the map's terrain, so none leads into a mass.
		Box->bDynamicObstacle = true;
		Box->SetAreaClassOverride(UNavArea_Null::StaticClass());
		Box->SetCanEverAffectNavigation(bNavigates);
		Box->SetupAttachment(RootComponent);
		Box->SetWorldLocationAndRotation(Volume.Centre, Facing.Rotation());
		Box->SetBoxExtent(FVector(Volume.Thickness / 2.0, Volume.Length / 2.0, Volume.HalfHeight), /*bUpdateOverlaps*/ false);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Box->RegisterComponent();
		Boxes.Add(Box);
	}
}
