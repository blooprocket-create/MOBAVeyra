// Copyright © 2026 Wayfinder Studios. All rights reserved.
#include "VeyraWorldBuild.h"
#include "Engine/World.h"
#include "Dom/JsonObject.h"
#include "Materials/MaterialInterface.h"
#include "Layout/VeyraTerrainProfile.h"
#include "Tuning/VeyraWorldTuning.h"
#include "WaterBodyRiverActor.h"
#include "WaterBodyComponent.h"
#include "WaterSplineComponent.h"
#include "WaterSplineMetadata.h"
#include "WaterZoneActor.h"
#include "WaterRuntimeSettings.h"

namespace VeyraWorldBuild
{
bool River(UWorld& World, const FVeyraWorldTuning& Tuning, const FJsonObject& Style, FString& Error)
{
    FString MaterialPath;
    if (!Style.TryGetStringField(TEXT("riverMaterial"), MaterialPath))
    { Error = TEXT("River presentation requires riverMaterial."); return false; }
    auto* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
    if (!Material) { Error = TEXT("Cannot load river presentation material: ") + MaterialPath; return false; }
    FString StaticMaterialPath;
    if (!Style.TryGetStringField(TEXT("riverStaticMaterial"), StaticMaterialPath))
    { Error = TEXT("River presentation requires riverStaticMaterial."); return false; }
    auto* StaticMaterial = LoadObject<UMaterialInterface>(nullptr, *StaticMaterialPath);
    if (!StaticMaterial) { Error = TEXT("Cannot load river static material."); return false; }
    const auto& Terrain = Tuning.Layout.Terrain;
    AWaterZone* Zone = World.SpawnActor<AWaterZone>();
    Zone->SetActorLabel(TEXT("Crucible_WaterPresentation"));
    Zone->SetZoneExtent(FVector2D(Tuning.Layout.HalfExtent + Terrain.ExteriorWidth) * 2.0);
    // The two mirrored channels share one basin. Geometry and full widths come only from World.
    for (const bool bMirror : { false, true })
    {
        const auto Samples = VeyraTerrainProfile::River(Terrain, bMirror);
        if (Samples.Num() < 2) { Error = TEXT("River needs at least two samples."); return false; }
        FActorSpawnParameters Spawn;
        Spawn.CustomPreSpawnInitialization = [](AActor* Actor) {
            // Disable the landscape brush before WaterEditor observes the new actor.
            CastChecked<AWaterBodyRiver>(Actor)->GetWaterBodyComponent()->bAffectsLandscape = false;
        };
        AWaterBodyRiver* Body = World.SpawnActor<AWaterBodyRiver>(Spawn);
        Body->SetActorLabel(bMirror ? TEXT("Crucible_River_B") : TEXT("Crucible_River_A"));
        auto* Component = Body->GetWaterBodyComponent();
        Component->bAffectsLandscape = false;
        Component->SetWaterMaterial(Material);
        Component->SetWaterStaticMeshMaterial(StaticMaterial);
        Component->SetWaterBodyStaticMeshEnabled(true);
        Component->SetWaterInfoMaterial(GetDefault<UWaterRuntimeSettings>()->GetDefaultWaterInfoMaterial());
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetCanEverAffectNavigation(false);
        Component->SetWaterZoneOverride(Zone);
        UWaterSplineComponent* Spline = Body->GetWaterSpline();
        Spline->ClearSplinePoints(false);
        for (const auto& Sample : Samples)
        {
            Spline->AddSplinePoint(FVector(Sample.Point, Terrain.RiverSurfaceZ), ESplineCoordinateSpace::World, false);
        }
        Spline->SetClosedLoop(false, false);
        Spline->UpdateSpline();
        UWaterSplineMetadata* Metadata = Body->GetWaterSplineMetadata();
        Metadata->Depth.Reset();
        Metadata->RiverWidth.Reset();
        Metadata->WaterVelocityScalar.Reset();
        for (int32 I = 0; I < Samples.Num(); ++I)
        {
            Spline->SetSplinePointType(I, ESplinePointType::Linear, false);
            Metadata->Depth.AddPoint(I, Terrain.RiverSurfaceZ - Terrain.RiverBedZ);
            // Water's width curve is a radius; the Veyra contract stores full widths.
            Metadata->RiverWidth.AddPoint(I, Samples[I].Width / 2.0);
            Metadata->WaterVelocityScalar.AddPoint(I, Terrain.RiverFlowSpeed);
        }
        Spline->UpdateSpline();
        FOnWaterBodyChangedParams Changed;
        Changed.bShapeOrPositionChanged = true;
        Component->OnWaterBodyChanged(Changed);
    }
    return true;
}
}
