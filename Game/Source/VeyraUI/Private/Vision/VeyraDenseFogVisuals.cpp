// Copyright © 2026 Wayfinder Studios. All rights reserved.
#include "Vision/VeyraDenseFogVisuals.h"

#include "Components/LocalFogVolumeComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Terrain/VeyraSurfacePlacement.h"

FName VeyraDenseFogVisuals::AuthoredTag()
{
	return TEXT("Veyra.AuthoredDenseFog");
}

bool VeyraDenseFogVisuals::HasAuthoredVisuals(const UWorld& World)
{
	for (TActorIterator<AActor> It(&World); It; ++It)
	{
		if (It->ActorHasTag(AuthoredTag()))
		{
			return true;
		}
	}
	return false;
}

bool VeyraDenseFogVisuals::Add(AActor& Owner, const FVector2D& Center, double Radius, const FVeyraSurfaceTuning& Surface)
{
	UWorld* World = Owner.GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}
	const auto& Style = *GetDefault<UVeyraDenseFogVisualSettings>();
	if (!ensureMsgf(FMath::IsFinite(Radius) && Radius > 0.0 && FMath::IsFinite(Style.RadialDensity) && Style.RadialDensity > 0.0f
		&& FMath::IsFinite(Style.HeightDensity) && Style.HeightDensity >= 0.0f
		&& FMath::IsFinite(Style.HeightFalloff) && Style.HeightFalloff >= 1.0f
		&& FMath::IsFinite(Style.WispLiftRatio) && Style.WispLiftRatio >= 0.0f && !Style.Wisps.IsNull(),
		TEXT("Dense Fog visual profile needs valid densities and height falloff.")))
	{
		return false;
	}
	FVector Location;
	if (!VeyraSurfacePlacement::Drape(*World, Center, Surface, Location))
	{
		return false;
	}
	ULocalFogVolumeComponent* Fog = NewObject<ULocalFogVolumeComponent>(&Owner);
	Owner.AddInstanceComponent(Fog);
	Fog->SetWorldLocation(Location);
	Fog->SetWorldScale3D(FVector(Radius / ULocalFogVolumeComponent::GetBaseVolumeSize()));
	Fog->SetRadialFogExtinction(Style.RadialDensity);
	Fog->SetHeightFogExtinction(Style.HeightDensity);
	Fog->SetHeightFogFalloff(Style.HeightFalloff);
	Fog->SetFogAlbedo(Style.Albedo);
	Fog->RegisterComponent();
	UNiagaraSystem* System = Style.Wisps.LoadSynchronous();
	if (!ensureMsgf(System, TEXT("Dense Fog requires its generated toon wisp system.")))
	{
		return false;
	}
	UNiagaraComponent* Wisps = NewObject<UNiagaraComponent>(&Owner);
	Owner.AddInstanceComponent(Wisps);
	// Generated map components must also start when the saved map is loaded.
	Wisps->SetAutoActivate(true);
	Wisps->SetAsset(System);
	Wisps->SetWorldLocation(Location + FVector(0.0, 0.0, Radius * Style.WispLiftRatio));
	Wisps->SetVariableFloat(TEXT("User.Scale"), static_cast<float>(Radius));
	Wisps->SetVariableLinearColor(TEXT("User.Color"), Style.Albedo);
	Wisps->RegisterComponent();
	Wisps->Activate(true);
	return true;
}
