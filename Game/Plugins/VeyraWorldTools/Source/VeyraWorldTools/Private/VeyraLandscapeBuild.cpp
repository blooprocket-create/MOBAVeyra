// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraWorldBuild.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Landscape.h"
#include "LandscapeDataAccess.h"
#include "LandscapeLayerInfoObject.h"
#include "Layout/VeyraTerrainField.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/PackageName.h"
#include "Terrain/VeyraGround.h"
#include "Tuning/VeyraWorldTuning.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
	/** A cubic ease from 0 to 1 as T runs from 0 to 1: the shape of the vista's rise, not a tuning curve. */
	double Smooth(double T)
	{
		const double Clamped = FMath::Clamp(T, 0.0, 1.0);
		return Clamped * Clamped * (3.0 - 2.0 * Clamped);
	}

	/** Octaves of the vista's ridged relief, each half as broad and as tall as the last: a fractal's shape. */
	constexpr int32 VistaOctaves = 4;

	/** Ridged fractal noise in [0, 1]: sharp crests, as mountain ranges have. */
	double Ridged(const FVector2D& Point, double Wavelength, int32 Seed)
	{
		double Total = 0.0;
		double Amplitude = 1.0;
		double Norm = 0.0;
		double Frequency = 1.0 / Wavelength;
		for (int32 Octave = 0; Octave < VistaOctaves; ++Octave)
		{
			const FVector2D Offset(Seed * 13.7 + Octave * 31.1, Seed * -7.3 + Octave * 17.9);
			Total += Amplitude * (1.0 - FMath::Abs(FMath::PerlinNoise2D(Point * Frequency + Offset)));
			Norm += Amplitude;
			Amplitude *= 0.5;
			Frequency *= 2.0;
		}
		return Total / Norm;
	}

	/** The asset at Path, loaded, or made and saved: generated output the map refers to, never hand edited. */
	ULandscapeLayerInfoObject* LayerInfo(const FString& Folder, const FName& Layer)
	{
		const FString Name = FString::Printf(TEXT("LI_Crucible_%s"), *Layer.ToString());
		const FString PackageName = Folder / Name;
		ULandscapeLayerInfoObject* Existing = FPackageName::DoesPackageExist(PackageName)
			? LoadObject<ULandscapeLayerInfoObject>(nullptr, *(PackageName + TEXT(".") + Name))
			: nullptr;
		if (Existing)
		{
			return Existing;
		}
		UPackage* Package = CreatePackage(*PackageName);
		ULandscapeLayerInfoObject* Info = NewObject<ULandscapeLayerInfoObject>(Package, *Name, RF_Public | RF_Standalone);
		Info->SetLayerName(Layer, false);
		FSavePackageArgs Save;
		Save.TopLevelFlags = RF_Public | RF_Standalone;
		const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		return UPackage::SavePackage(Package, Info, *Filename, Save) ? Info : nullptr;
	}
}

namespace VeyraWorldBuild
{
	FVeyraTerrainRelief ReliefOf(const FJsonObject& Style)
	{
		FVeyraTerrainRelief Relief;
		Style.TryGetNumberField(TEXT("reliefAmplitude"), Relief.Amplitude);
		Style.TryGetNumberField(TEXT("reliefWavelength"), Relief.Wavelength);
		Style.TryGetNumberField(TEXT("crestAmplitude"), Relief.CrestAmplitude);
		Style.TryGetNumberField(TEXT("crestWavelengthShare"), Relief.CrestWavelengthShare);
		Relief.Seed = static_cast<int32>(Style.GetNumberField(TEXT("seed")));
		return Relief;
	}

	bool Landscape(UWorld& World, const FVeyraWorldTuning& Tuning, const FJsonObject& Style, FString& Error)
	{
		const int32 Components = static_cast<int32>(Style.GetNumberField(TEXT("landscapeComponents")));
		const int32 Quads = static_cast<int32>(Style.GetNumberField(TEXT("landscapeQuadsPerComponent")));
		const int32 Size = Components * Quads + 1;
		const double ZScale = Style.GetNumberField(TEXT("landscapeVerticalScale"));
		const double VistaWidth = Style.GetNumberField(TEXT("vistaWidth"));
		const double VistaAmplitude = Style.GetNumberField(TEXT("vistaAmplitude"));
		const double VistaWavelength = Style.GetNumberField(TEXT("vistaWavelength"));
		const double VistaRise = Style.GetNumberField(TEXT("vistaRise"));
		const double WetBand = Style.GetNumberField(TEXT("wetBand"));
		const double BankBand = Style.GetNumberField(TEXT("bankBand"));
		FString MaterialPath;
		FString LayerFolder;
		if (Components < 1 || Components > 32 || Quads != 63 || ZScale <= 0.0 || VistaWidth < 0.0 || VistaAmplitude < 0.0 || VistaWavelength <= 0.0
			|| VistaRise <= 0.0 || WetBand < 0.0 || BankBand <= 0.0 || !Style.TryGetStringField(TEXT("landscapeMaterial"), MaterialPath)
			|| !Style.TryGetStringField(TEXT("landscapeLayerFolder"), LayerFolder))
		{
			Error = TEXT("Invalid Landscape profile: sampling, vista, wet band, material and layer folder.");
			return false;
		}
		UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
		if (!Material)
		{
			Error = TEXT("Build the terrain art first (BuildTerrainArt.ps1): cannot load ") + MaterialPath;
			return false;
		}

		// The floor, the rim beyond it, and an inaccessible vista beyond that, so the view never ends in nothing.
		const FVeyraTerrainTuning& Terrain = Tuning.Layout.Terrain;
		const double Rim = Tuning.Layout.HalfExtent + Terrain.BoundaryWidth;
		const double Extent = Rim + VistaWidth;
		const double Spacing = Extent * 2.0 / (Size - 1);
		ALandscape* Ground = World.SpawnActor<ALandscape>();
		Ground->SetActorLabel(TEXT("Crucible_Landscape"));
		Ground->Tags.Add(TEXT("Veyra.AuthoredTerrain"));
		Ground->SetActorTransform(FTransform(FRotator::ZeroRotator, FVector(-Extent, -Extent, 0.0), FVector(Spacing, Spacing, ZScale)));
		// Playable ground (ADR-040 §4): what ground queries find, and what sweeps for walls pass over.
		Ground->BodyInstance.SetCollisionProfileName(VeyraGround::ProfileName());

		// The material's water's edge follows the river's surface, from World, the one spatial authority.
		UMaterialInstanceConstant* Instance = NewObject<UMaterialInstanceConstant>(World.GetPackage(), TEXT("MI_CrucibleTerrain"), RF_Public | RF_Standalone);
		Instance->SetParentEditorOnly(Material);
		Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("WaterLevel")), static_cast<float>(Tuning.Layout.River.SurfaceZ));
		Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("WetBand")), static_cast<float>(WetBand));
		Instance->PostEditChange();
		Ground->LandscapeMaterial = Instance;

		// The painted layers. Cliff is never painted: the material lays rock on whatever is steep.
		const TArray<FName> Names = { TEXT("Jungle"), TEXT("Lane"), TEXT("Bank"), TEXT("Cliff") };
		TArray<FLandscapeImportLayerInfo> Layers;
		for (const FName& Name : Names)
		{
			FLandscapeImportLayerInfo& Layer = Layers.Emplace_GetRef(Name);
			Layer.LayerInfo = LayerInfo(LayerFolder, Name);
			if (!Layer.LayerInfo)
			{
				Error = TEXT("Cannot save the Landscape layer info for ") + Name.ToString();
				return false;
			}
			Layer.LayerData.SetNumUninitialized(Size * Size);
		}

		const FVeyraTerrainRelief Relief = ReliefOf(Style);
		const FVeyraTerrainField Field(Tuning, Relief);
		TArray<uint16> Heights;
		Heights.SetNumUninitialized(Size * Size);
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				const int32 Index = Y * Size + X;
				const FVector2D Point(-Extent + X * Spacing, -Extent + Y * Spacing);
				const FVeyraTerrainSample Sample = Field.Sample(Point);
				// Beyond the rim, mountains: presentation only, out of every unit's reach, open where the river leaves.
				const double Beyond = FMath::Max(FMath::Abs(Point.X), FMath::Abs(Point.Y)) - Rim;
				const double Vista = Beyond > 0.0 ? Sample.Rim * Smooth(Beyond / VistaRise) * VistaAmplitude * Ridged(Point, VistaWavelength, Relief.Seed) : 0.0;
				Heights[Index] = LandscapeDataAccess::GetTexHeight((Sample.Height + Vista) / ZScale);
				// Weights in order of precedence: the water's shore, then roads and pads, the jungle the rest. Ridge tops,
				// the rim and the vista stay jungle, overgrown; the material lays rock on whatever is steep.
				const double Bank = 1.0 - Smooth(Sample.WaterDistance / BankBand);
				const double Road = (1.0 - Bank) * FMath::Max(Sample.Road, Sample.Pad);
				const int32 B = FMath::Clamp(FMath::RoundToInt(Bank * 255.0), 0, 255);
				const int32 R = FMath::Clamp(FMath::RoundToInt(Road * 255.0), 0, 255 - B);
				Layers[0].LayerData[Index] = static_cast<uint8>(255 - B - R);
				Layers[1].LayerData[Index] = static_cast<uint8>(R);
				Layers[2].LayerData[Index] = static_cast<uint8>(B);
				Layers[3].LayerData[Index] = 0;
			}
		}
		TMap<FGuid, TArray<uint16>> HeightData;
		HeightData.Add(FGuid(), MoveTemp(Heights));
		// The Landscape knows its layers before they are painted. Import registers the components before it declares their
		// layers, and the engine's fix-up treats a weightmap of a layer the Landscape does not know as unused: it deletes it
		// and re-merges every component through a render path the pinned engine crashes on in a commandlet world.
		for (const FLandscapeImportLayerInfo& Layer : Layers)
		{
			Ground->AddTargetLayer(Layer.LayerName, FLandscapeTargetLayerSettings(Layer.LayerInfo), /*bPostEditChange*/ false);
		}
		TMap<FGuid, TArray<FLandscapeImportLayerInfo>> LayerData;
		LayerData.Add(FGuid(), MoveTemp(Layers));
		// The same Landscape identity every generation, from the profile's seed, so regenerating changes only what changed.
		const uint32 Seed = static_cast<uint32>(Relief.Seed);
		const FGuid Identity(0x56657972, 0x61437275, 0x6369626C, Seed);
		Ground->Import(Identity, 0, 0, Size - 1, Size - 1, 1, Quads, HeightData, nullptr, LayerData, ELandscapeImportAlphamapType::Additive, {});
		Ground->ForceLayersFullUpdate();
		Ground->UpdateAllComponentMaterialInstances();
		Ground->RecreateCollisionComponents();
		Ground->PostEditChange();
		return true;
	}
}
