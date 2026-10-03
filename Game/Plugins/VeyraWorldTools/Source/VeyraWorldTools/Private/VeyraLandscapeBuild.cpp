// Copyright © 2026 Wayfinder Studios. All rights reserved.
#include "VeyraWorldBuild.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Landscape.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeDataAccess.h"
#include "Layout/VeyraLayout.h"
#include "Layout/VeyraTerrainProfile.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionLandscapeLayerBlend.h"
#include "Tuning/VeyraWorldTuning.h"

namespace VeyraWorldBuild
{
bool Landscape(UWorld& World, const FVeyraWorldTuning& Tuning, const FJsonObject& Style, FString& Error)
{
    const int32 Components = static_cast<int32>(Style.GetNumberField(TEXT("landscapeComponents")));
    const int32 Quads = static_cast<int32>(Style.GetNumberField(TEXT("landscapeQuadsPerComponent")));
    const int32 Size = Components * Quads + 1;
    const double ZScale = Style.GetNumberField(TEXT("landscapeVerticalScale"));
    if (Components < 1 || Components > 32 || Quads != 63 || ZScale <= 0.0)
    {
        Error = TEXT("Invalid Landscape sampling profile.");
        return false;
    }
    const double Extent = Tuning.Layout.HalfExtent + Tuning.Layout.Terrain.ExteriorWidth;
    const double Spacing = Extent * 2.0 / (Size - 1);
    ALandscape* Ground = World.SpawnActor<ALandscape>();
    Ground->SetActorLabel(TEXT("Crucible_Landscape"));
    Ground->Tags.Add(TEXT("Veyra.AuthoredTerrain"));
    Ground->SetActorTransform(FTransform(FRotator::ZeroRotator, FVector(-Extent, -Extent, 0.0), FVector(Spacing, Spacing, ZScale)));

    // Assets are embedded in the generated map; their source is this tool and its style profile.
    UMaterial* Material = NewObject<UMaterial>(World.GetPackage(), TEXT("M_CrucibleTerrain"), RF_Public | RF_Standalone);
    auto* Blend = NewObject<UMaterialExpressionLandscapeLayerBlend>(Material);
    Material->GetExpressionCollection().AddExpression(Blend);
    const TArray<TSharedPtr<FJsonValue>>& Colors = Style.GetArrayField(TEXT("groundColors"));
    const TArray<FName> Names = { TEXT("Jungle"), TEXT("Lane"), TEXT("Bank"), TEXT("Cliff") };
    if (Colors.Num() != Names.Num()) { Error = TEXT("Expected one color for each terrain layer."); return false; }
    TArray<FLandscapeImportLayerInfo> Layers;
    for (int32 I = 0; I < Names.Num(); ++I)
    {
        const auto& Color = Colors[I]->AsArray();
        if (Color.Num() != 3) { Error = TEXT("Terrain colors must have three linear components."); return false; }
        FLayerBlendInput& Input = Blend->Layers.AddDefaulted_GetRef();
        Input.LayerName = Names[I];
        Input.BlendType = LB_WeightBlend;
        Input.ConstLayerInput = FVector(Color[0]->AsNumber(), Color[1]->AsNumber(), Color[2]->AsNumber());
        FLandscapeImportLayerInfo& Layer = Layers.Emplace_GetRef(Names[I]);
        Layer.LayerInfo = NewObject<ULandscapeLayerInfoObject>(World.GetPackage(), FName(*FString::Printf(TEXT("LI_Crucible_%s"), *Names[I].ToString())), RF_Public | RF_Standalone);
        Layer.LayerInfo->SetLayerName(Names[I], false);
        Layer.LayerData.SetNumUninitialized(Size * Size);
    }
    Material->GetEditorOnlyData()->BaseColor.Connect(0, Blend);
    auto* Roughness = NewObject<UMaterialExpressionConstant>(Material);
    Roughness->R = Style.GetNumberField(TEXT("roughness"));
    Material->GetExpressionCollection().AddExpression(Roughness);
    Material->GetEditorOnlyData()->Roughness.Connect(0, Roughness);
    Material->PostEditChange();
    Ground->LandscapeMaterial = Material;

    const FVeyraTerrainSampler Sampler(Tuning);
    TArray<uint16> Heights;
    Heights.SetNumUninitialized(Size * Size);
    for (int32 Y = 0; Y < Size; ++Y)
    {
        for (int32 X = 0; X < Size; ++X)
        {
            const int32 Index = Y * Size + X;
            const FVector2D Point(-Extent + X * Spacing, -Extent + Y * Spacing);
            Heights[Index] = LandscapeDataAccess::GetTexHeight(Sampler.Height(Point) / ZScale);
            double RoadDistance = TNumericLimits<double>::Max();
            for (const auto& Lane : Tuning.Layout.Lanes)
            {
                RoadDistance = FMath::Min(RoadDistance, VeyraLayout::DistanceToPath(Lane.Points, Point) - Lane.Width / 2.0);
            }
            for (const EVeyraTeam Team : { EVeyraTeam::A, EVeyraTeam::B })
            {
                RoadDistance = FMath::Min(RoadDistance, FVector2D::Distance(Point, VeyraLayout::ForTeam(VeyraLayout::ToVector(Tuning.Layout.Base.PrimeWell), Team)) - Tuning.Layout.Base.PadRadius);
            }
            const double Cliff = FMath::Clamp((FMath::Max(FMath::Abs(Point.X), FMath::Abs(Point.Y)) - Tuning.Layout.HalfExtent) / Tuning.Layout.Terrain.ExteriorWidth, 0.0, 1.0);
            const double Bank = (1.0 - Cliff) * (1.0 - FMath::Clamp(Sampler.RiverDistance(Point) / Tuning.Layout.Terrain.BankBlend, 0.0, 1.0));
            const double Road = (1.0 - Cliff - Bank) * (1.0 - FMath::Clamp(RoadDistance / Tuning.Layout.Terrain.LaneShoulder, 0.0, 1.0));
            const uint8 C = FMath::RoundToInt(Cliff * 255.0);
            const uint8 B = FMath::RoundToInt(Bank * (255 - C) / FMath::Max(1.0 - Cliff, UE_DOUBLE_SMALL_NUMBER));
            const uint8 R = FMath::Clamp(FMath::RoundToInt(Road * 255.0), 0, 255 - C - B);
            Layers[0].LayerData[Index] = 255 - C - B - R;
            Layers[1].LayerData[Index] = R;
            Layers[2].LayerData[Index] = B;
            Layers[3].LayerData[Index] = C;
        }
    }
    TMap<FGuid, TArray<uint16>> HeightData;
    HeightData.Add(FGuid(), MoveTemp(Heights));
    TMap<FGuid, TArray<FLandscapeImportLayerInfo>> LayerData;
    LayerData.Add(FGuid(), MoveTemp(Layers));
    Ground->Import(FGuid::NewGuid(), 0, 0, Size - 1, Size - 1, 1, Quads, HeightData, nullptr, LayerData, ELandscapeImportAlphamapType::Additive, {});
    Ground->ForceLayersFullUpdate();
    Ground->UpdateAllComponentMaterialInstances();
    Ground->RecreateCollisionComponents();
    Ground->PostEditChange();
    return true;
}
}
