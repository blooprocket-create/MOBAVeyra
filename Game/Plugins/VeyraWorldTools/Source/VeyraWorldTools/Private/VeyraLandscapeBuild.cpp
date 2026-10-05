// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraWorldBuild.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Landscape.h"
#include "LandscapeDataAccess.h"
#include "LandscapeLayerInfoObject.h"
#include "Layout/VeyraTerrainField.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionLandscapeLayerBlend.h"
#include "Tuning/VeyraWorldTuning.h"

namespace VeyraWorldBuild
{
	FVeyraTerrainRelief ReliefOf(const FJsonObject& Style)
	{
		FVeyraTerrainRelief Relief;
		Style.TryGetNumberField(TEXT("reliefAmplitude"), Relief.Amplitude);
		Style.TryGetNumberField(TEXT("reliefWavelength"), Relief.Wavelength);
		Style.TryGetNumberField(TEXT("crestAmplitude"), Relief.CrestAmplitude);
		Relief.Seed = static_cast<int32>(Style.GetNumberField(TEXT("seed")));
		return Relief;
	}

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
		// The floor and the rim beyond it.
		const double Extent = Tuning.Layout.HalfExtent + Tuning.Layout.Terrain.BoundaryWidth;
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
		if (Colors.Num() != Names.Num())
		{
			Error = TEXT("Expected one color for each terrain layer.");
			return false;
		}
		TArray<FLandscapeImportLayerInfo> Layers;
		for (int32 Index = 0; Index < Names.Num(); ++Index)
		{
			const TArray<TSharedPtr<FJsonValue>>& Color = Colors[Index]->AsArray();
			if (Color.Num() != 3)
			{
				Error = TEXT("Terrain colors must have three linear components.");
				return false;
			}
			FLayerBlendInput& Input = Blend->Layers.AddDefaulted_GetRef();
			Input.LayerName = Names[Index];
			Input.BlendType = LB_WeightBlend;
			Input.ConstLayerInput = FVector(Color[0]->AsNumber(), Color[1]->AsNumber(), Color[2]->AsNumber());
			FLandscapeImportLayerInfo& Layer = Layers.Emplace_GetRef(Names[Index]);
			Layer.LayerInfo = NewObject<ULandscapeLayerInfoObject>(World.GetPackage(), FName(*FString::Printf(TEXT("LI_Crucible_%s"), *Names[Index].ToString())), RF_Public | RF_Standalone);
			Layer.LayerInfo->SetLayerName(Names[Index], false);
			Layer.LayerData.SetNumUninitialized(Size * Size);
		}
		Material->GetEditorOnlyData()->BaseColor.Connect(0, Blend);
		auto* Roughness = NewObject<UMaterialExpressionConstant>(Material);
		Roughness->R = Style.GetNumberField(TEXT("roughness"));
		Material->GetExpressionCollection().AddExpression(Roughness);
		Material->GetEditorOnlyData()->Roughness.Connect(0, Roughness);
		Material->PostEditChange();
		Ground->LandscapeMaterial = Material;

		const FVeyraTerrainField Field(Tuning, ReliefOf(Style));
		const double BankWidth = Tuning.Layout.Terrain.BankWidth;
		TArray<uint16> Heights;
		Heights.SetNumUninitialized(Size * Size);
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				const int32 Index = Y * Size + X;
				const FVeyraTerrainSample Sample = Field.Sample(FVector2D(-Extent + X * Spacing, -Extent + Y * Spacing));
				Heights[Index] = LandscapeDataAccess::GetTexHeight(Sample.Height / ZScale);
				// Weights in order of precedence: rock, then the water's banks, then roads and pads, the jungle the rest.
				const double Cliff = FMath::Max(Sample.Rim, Sample.Ridge);
				const double Bank = (1.0 - Cliff) * (1.0 - FMath::Clamp(Sample.WaterDistance / BankWidth, 0.0, 1.0));
				const double Road = (1.0 - Cliff - Bank) * FMath::Max(Sample.Road, Sample.Pad);
				const uint8 C = static_cast<uint8>(FMath::RoundToInt(Cliff * 255.0));
				const uint8 B = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Bank * 255.0), 0, 255 - C));
				const uint8 R = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Road * 255.0), 0, 255 - C - B));
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
