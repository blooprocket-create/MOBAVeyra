// Copyright © 2026 Wayfinder Studios. All rights reserved.
#include "VeyraWorldBuild.h"
#include "Dom/JsonObject.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "Layout/VeyraLayout.h"
#include "Layout/VeyraTerrainProfile.h"
#include "Tuning/VeyraWorldTuning.h"

namespace VeyraWorldBuild
{
void ReviewScene(UWorld& World, const FVeyraWorldTuning& Tuning, const FJsonObject& Style)
{
    const FRotator LightRotation(Style.GetNumberField(TEXT("sunPitch")), Style.GetNumberField(TEXT("sunYaw")), 0.0);
    auto* Sun = World.SpawnActor<ADirectionalLight>(FVector::ZeroVector, LightRotation);
    Sun->SetActorLabel(TEXT("Crucible_Sun"));
    Sun->GetRootComponent()->SetMobility(EComponentMobility::Movable);
    auto* Light = CastChecked<UDirectionalLightComponent>(Sun->GetLightComponent());
    Light->SetIntensity(Style.GetNumberField(TEXT("sunLux")));
    Light->bAtmosphereSunLight = true;
    World.SpawnActor<ASkyAtmosphere>();
    auto* Sky = World.SpawnActor<ASkyLight>();
    Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Sky->GetLightComponent()->SetIntensity(Style.GetNumberField(TEXT("skyIntensity")));
    Sky->GetLightComponent()->SetRealTimeCapture(true);
    auto* Exposure = World.SpawnActor<APostProcessVolume>();
    Exposure->bUnbound = true;
    // Explicit manual exposure is independent of the project's extended-luminance toggle.
    Exposure->Settings.bOverride_AutoExposureMethod = true;
    Exposure->Settings.AutoExposureMethod = AEM_Manual;
    Exposure->Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
    Exposure->Settings.AutoExposureApplyPhysicalCameraExposure = false;
    Exposure->Settings.bOverride_AutoExposureBias = true;
    Exposure->Settings.AutoExposureBias = -Style.GetNumberField(TEXT("exposureEV100"));

    const FVeyraTerrainSampler Terrain(Tuning);
    auto Camera = [&](const FString& Name, const FVector2D& Point, bool Reverse = false, bool Overview = false)
    {
        const double Height = Style.GetNumberField(Overview ? TEXT("overviewHeight") : TEXT("reviewHeight"));
        const FRotator Rotation(Overview ? -90.0 : Style.GetNumberField(TEXT("reviewPitch")), Style.GetNumberField(TEXT("reviewYaw")) + (Reverse ? 180.0 : 0.0), 0.0);
        const FVector Target(Point, Terrain.Height(Point));
        const FVector Position = Target - Rotation.Vector() * (Height / -Rotation.Vector().Z);
        auto* View = World.SpawnActor<ACameraActor>(Position, Rotation);
        View->SetActorLabel(TEXT("Review_") + Name);
        View->Tags.Add(TEXT("Veyra.ReviewCamera"));
        View->GetCameraComponent()->FieldOfView = Style.GetNumberField(TEXT("reviewFOV"));
    };
    Camera(TEXT("Overview"), FVector2D::ZeroVector, false, true);
    for (const auto& Lane : Tuning.Layout.Lanes)
    {
        const FVector2D Middle = VeyraLayout::PointAlong(Lane.Points, VeyraLayout::Length(Lane.Points) / 2.0);
        const FString Name = UEnum::GetValueAsString(Lane.Lane).RightChop(FString(TEXT("EVeyraLane::")).Len());
        Camera(Name + TEXT("_A"), Middle);
        Camera(Name + TEXT("_B"), Middle, true);
        Camera(TEXT("Crossing_") + Name, Middle);
    }
    for (const EVeyraTeam Team : { EVeyraTeam::A, EVeyraTeam::B })
    {
        const FString Side = Team == EVeyraTeam::A ? TEXT("A") : TEXT("B");
        const FVector2D Base = VeyraLayout::ForTeam(VeyraLayout::ToVector(Tuning.Layout.Base.PrimeWell), Team);
        Camera(TEXT("PrimeWell_") + Side, Base, Team == EVeyraTeam::B);
        Camera(TEXT("BaseApproach_") + Side, Base * (1.0 - Tuning.Layout.Base.PadRadius / Base.Size()), Team == EVeyraTeam::B);
        if (!Tuning.Wildlife.Camps.IsEmpty()) { Camera(TEXT("InnerJungle_") + Side, VeyraLayout::ForTeam(VeyraLayout::ToVector(Tuning.Wildlife.Camps[0].Center), Team)); }
    }
    for (int32 I = 0; I < Tuning.FluxWells.Sites.Num(); ++I) { Camera(FString::Printf(TEXT("FluxWell_%d"), I), VeyraLayout::ToVector(Tuning.FluxWells.Sites[I])); }
    if (!Tuning.Layout.DenseFog.IsEmpty()) { Camera(TEXT("DenseFog"), VeyraLayout::ToVector(Tuning.Layout.DenseFog[0].Center)); }
    for (const auto& Camp : Tuning.Wildlife.Camps)
    {
        const FVector2D Point = VeyraLayout::ToVector(Camp.Center);
        if (FMath::Max(FMath::Abs(Point.X), FMath::Abs(Point.Y)) > Tuning.Layout.HalfExtent - Tuning.Layout.Terrain.ExteriorWidth)
        {
            Camera(TEXT("OuterJungle"), Point);
            Camera(TEXT("FoliageBenchmark"), Point);
            break;
        }
    }
    Camera(TEXT("CombatBenchmark"), FVector2D::ZeroVector);
}
}
