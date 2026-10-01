// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Terrain/VeyraTerrainWall.h"

#include "Components/BoxComponent.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

AVeyraTerrainWall::AVeyraTerrainWall()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	// Terrain is map knowledge, and every machine's movement must meet it.
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);

	// World-static, so movement, forced moves and line projectiles meet it as terrain; never the cursor's
	// trace, the camera or the markers' channel.
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Box->SetCollisionObjectType(ECC_WorldStatic);
	Box->SetCollisionResponseToAllChannels(ECR_Block);
	Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
	Box->SetGenerateOverlapEvents(false);
	Box->SetCanEverAffectNavigation(false);
	Box->bDynamicObstacle = true;
	RootComponent = Box;
}

void AVeyraTerrainWall::SetHalfExtent(const FVector& InHalfExtent)
{
	HalfExtent = FVector3f(InHalfExtent);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraTerrainWall, HalfExtent, this);
}

void AVeyraTerrainWall::BeginPlay()
{
	Super::BeginPlay();
	ApplyHalfExtent();
}

void AVeyraTerrainWall::ApplyHalfExtent()
{
	if (HalfExtent.GetMin() <= 0.0f || Box->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
	{
		return;
	}
	Box->SetBoxExtent(FVector(HalfExtent), /*bUpdateOverlaps*/ false);
	Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	// Paths go round it: the server's navigation takes it as an obstacle, and drops it as it goes.
	if (HasAuthority())
	{
		Box->SetCanEverAffectNavigation(true);
	}
}

void AVeyraTerrainWall::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraTerrainWall, HalfExtent, Params);
}
