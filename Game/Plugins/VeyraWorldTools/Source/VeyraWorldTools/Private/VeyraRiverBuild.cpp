// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraWorldBuild.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Layout/VeyraRiver.h"
#include "Materials/MaterialInterface.h"
#include "Tuning/VeyraWorldTuning.h"
#include "WaterBodyComponent.h"
#include "WaterBodyRiverActor.h"
#include "WaterRuntimeSettings.h"
#include "WaterSplineComponent.h"
#include "WaterSplineMetadata.h"
#include "WaterZoneActor.h"

namespace VeyraWorldBuild
{
	bool River(UWorld& World, const FVeyraWorldTuning& Tuning, const FJsonObject& Style, FString& Error)
	{
		FString MaterialPath;
		FString StaticMaterialPath;
		if (!Style.TryGetStringField(TEXT("riverMaterial"), MaterialPath) || !Style.TryGetStringField(TEXT("riverStaticMaterial"), StaticMaterialPath))
		{
			Error = TEXT("River presentation requires riverMaterial and riverStaticMaterial.");
			return false;
		}
		UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
		UMaterialInterface* StaticMaterial = LoadObject<UMaterialInterface>(nullptr, *StaticMaterialPath);
		if (!Material || !StaticMaterial)
		{
			Error = TEXT("Cannot load the river's presentation materials.");
			return false;
		}
		const FVeyraRiverLayout& Water = Tuning.Layout.River;
		AWaterZone* Zone = World.SpawnActor<AWaterZone>();
		Zone->SetActorLabel(TEXT("Crucible_WaterPresentation"));
		Zone->SetZoneExtent(FVector2D(Tuning.Layout.HalfExtent + Tuning.Layout.Terrain.BoundaryWidth) * 2.0);
		// One water body to each channel: the main channel through the centre, then each island's and its rotation. Their
		// courses and full widths come only from World; the water draws, it never blocks or decides (ADR-040 §6).
		const TArray<FVeyraRiverChannel>& Channels = VeyraRiver::ShapeOf(Tuning.Layout).GetChannels();
		for (int32 ChannelIndex = 0; ChannelIndex < Channels.Num(); ++ChannelIndex)
		{
			const FVeyraRiverChannel& Channel = Channels[ChannelIndex];
			if (Channel.Samples.Num() < 2)
			{
				Error = TEXT("Every river channel needs at least two samples.");
				return false;
			}
			FActorSpawnParameters Spawn;
			Spawn.CustomPreSpawnInitialization = [](AActor* Actor) {
				// The terrain field cuts the basin; the water must not carve the Landscape again.
				CastChecked<AWaterBodyRiver>(Actor)->GetWaterBodyComponent()->bAffectsLandscape = false;
			};
			AWaterBodyRiver* Body = World.SpawnActor<AWaterBodyRiver>(Spawn);
			Body->SetActorLabel(FString::Printf(TEXT("Crucible_River_%s_%d"), *Channel.Id.ToString(), ChannelIndex));
			UWaterBodyComponent* Component = Body->GetWaterBodyComponent();
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
			for (const FVeyraRiverSample& Sample : Channel.Samples)
			{
				Spline->AddSplinePoint(FVector(Sample.Point, Water.SurfaceZ), ESplineCoordinateSpace::World, false);
			}
			Spline->SetClosedLoop(false, false);
			UWaterSplineMetadata* Metadata = Body->GetWaterSplineMetadata();
			Metadata->Depth.Reset();
			Metadata->RiverWidth.Reset();
			Metadata->WaterVelocityScalar.Reset();
			for (int32 Index = 0; Index < Channel.Samples.Num(); ++Index)
			{
				// Already sampled along the river's curve: straight spans between samples follow it.
				Spline->SetSplinePointType(Index, ESplinePointType::Linear, false);
				Metadata->Depth.AddPoint(Index, Water.SurfaceZ - Water.BedZ);
				// The river component halves this width itself: full widths, as World stores them.
				Metadata->RiverWidth.AddPoint(Index, Channel.Samples[Index].Width);
				Metadata->WaterVelocityScalar.AddPoint(Index, Water.FlowSpeed);
			}
			Spline->UpdateSpline();
			FOnWaterBodyChangedParams Changed;
			Changed.bShapeOrPositionChanged = true;
			Component->OnWaterBodyChanged(Changed);
		}
		return true;
	}
}
