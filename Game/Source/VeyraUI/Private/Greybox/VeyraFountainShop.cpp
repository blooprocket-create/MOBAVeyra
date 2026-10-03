// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Greybox/VeyraFountainShop.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

AVeyraFountainShop::AVeyraFountainShop()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

namespace
{
	/** Its parts' proportions, as shares of its radius and height: a broad low base, a pillar, and a crown atop. */
	constexpr double BaseHeightShare = 0.12;
	constexpr double PillarRadiusShare = 0.35;
	constexpr double CrownRadiusShare = 0.6;
}

void AVeyraFountainShop::Build(UStaticMesh& Column, UStaticMesh& Crown, UMaterialInterface& Material, FName ColorParameter, const FLinearColor& Color, double Radius,
	double InHeight)
{
	Height = InHeight;
	const double BaseHeight = Height * BaseHeightShare;
	const double CrownRadius = Radius * CrownRadiusShare;
	const double PillarHeight = Height - BaseHeight - 2.0 * CrownRadius;
	AddPart(Column, Material, ColorParameter, Color, FVector(Radius, Radius, BaseHeight / 2.0), 0.0);
	AddPart(Column, Material, ColorParameter, Color, FVector(Radius * PillarRadiusShare, Radius * PillarRadiusShare, PillarHeight / 2.0), BaseHeight);
	AddPart(Crown, Material, ColorParameter, Color, FVector(CrownRadius), BaseHeight + PillarHeight);
}

UStaticMeshComponent* AVeyraFountainShop::AddPart(UStaticMesh& Mesh, UMaterialInterface& Material, FName ColorParameter, const FLinearColor& Color, const FVector& Size, double Base)
{
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
	Part->SetStaticMesh(&Mesh);
	Part->SetMobility(EComponentMobility::Movable);
	// The cursor's trace finds it; nothing else does, and no unit's way bends round it.
	Part->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Part->SetCollisionResponseToAllChannels(ECR_Ignore);
	Part->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Part->SetGenerateOverlapEvents(false);
	Part->SetCanEverAffectNavigation(false);
	Part->SetupAttachment(Root);
	// Fitted to Size, standing on Base above the ground.
	const FBoxSphereBounds Bounds = Mesh.GetBounds();
	const FVector Scale = Size / Bounds.BoxExtent.ComponentMax(FVector(UE_KINDA_SMALL_NUMBER));
	Part->SetRelativeScale3D(Scale);
	Part->SetRelativeLocation(-Bounds.Origin * Scale + FVector(0.0, 0.0, Base + Size.Z));
	Part->RegisterComponent();
	if (UMaterialInstanceDynamic* Tinted = Part->CreateDynamicMaterialInstance(0, &Material))
	{
		Tinted->SetVectorParameterValue(ColorParameter, Color);
	}
	Parts.Add(Part);
	return Part;
}

void AVeyraFountainShop::SetOutlined(bool bOutlined, int32 Stencil)
{
	for (UStaticMeshComponent* Part : Parts)
	{
		if (Part->bRenderCustomDepth != bOutlined)
		{
			Part->SetRenderCustomDepth(bOutlined);
		}
		if (bOutlined && Part->CustomDepthStencilValue != Stencil)
		{
			Part->SetCustomDepthStencilValue(Stencil);
		}
	}
}

bool AVeyraFountainShop::IsOutlined() const
{
	return !Parts.IsEmpty() && Parts[0]->bRenderCustomDepth;
}
