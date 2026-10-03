// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "Teams/VeyraTeam.h"

#include "VeyraFountainShop.generated.h"

class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * The shop standing at a side's fountain (ADR-063 §6): a pillar of engine shapes the grey-box presentation draws,
 * so a player sees there is a shop and can click it. Presentation only, on each client: it never replicates, blocks
 * nothing but the cursor's trace, shapes no navigation, and opening the shop from it changes nothing the shop's key
 * would not; buying stays the server's to allow, at the fountain.
 */
UCLASS(Transient, NotPlaceable)
class VEYRAUI_API AVeyraFountainShop : public AActor
{
	GENERATED_BODY()

public:
	AVeyraFountainShop();

	/** Builds its look: a base and a pillar of Column, crowned by Crown, Height tall and Radius wide, in Color. */
	void Build(UStaticMesh& Column, UStaticMesh& Crown, UMaterialInterface& Material, FName ColorParameter, const FLinearColor& Color, double Radius, double Height);

	EVeyraTeam GetTeam() const { return Team; }
	void SetTeam(EVeyraTeam InTeam) { Team = InTeam; }

	/** How tall it stands, for its label. */
	double GetHeight() const { return Height; }

	/** Outlines it with Stencil while bOutlined (ADR-063 §3). */
	void SetOutlined(bool bOutlined, int32 Stencil);

	/** Whether it is outlined now. */
	bool IsOutlined() const;

private:
	UStaticMeshComponent* AddPart(UStaticMesh& Mesh, UMaterialInterface& Material, FName ColorParameter, const FLinearColor& Color, const FVector& Size, double Base);

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	EVeyraTeam Team = EVeyraTeam::None;
	double Height = 0.0;
};
