// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraWorldBuild.h"

#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Layout/VeyraRiver.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"
#include "Tuning/VeyraWorldTuning.h"

namespace
{
	/** The downstream direction of Channel at its sample nearest Point, and how far that sample lies. */
	FVector2D FlowAlong(const FVeyraRiverChannel& Channel, const FVector2D& Point, double& OutDistance)
	{
		int32 Nearest = 0;
		OutDistance = TNumericLimits<double>::Max();
		for (int32 Index = 0; Index < Channel.Samples.Num(); ++Index)
		{
			const double Distance = FVector2D::Distance(Channel.Samples[Index].Point, Point) - Channel.Samples[Index].Width / 2.0;
			if (Distance < OutDistance)
			{
				OutDistance = Distance;
				Nearest = Index;
			}
		}
		const FVector2D& Before = Channel.Samples[FMath::Max(Nearest - 1, 0)].Point;
		const FVector2D& After = Channel.Samples[FMath::Min(Nearest + 1, Channel.Samples.Num() - 1)].Point;
		return (After - Before).GetSafeNormal();
	}
}

namespace VeyraWorldBuild
{
	bool River(UWorld& World, const FVeyraWorldTuning& Tuning, const FJsonObject& Style, FString& Error)
	{
		FString MaterialPath;
		const double Cell = Style.GetNumberField(TEXT("riverCellSize"));
		const double Margin = Style.GetNumberField(TEXT("riverSurfaceMargin"));
		const double ShoreWidth = Style.GetNumberField(TEXT("riverShoreWidth"));
		if (!Style.TryGetStringField(TEXT("riverMaterial"), MaterialPath) || !(Cell > 0.0) || Margin < 0.0 || !(ShoreWidth > 0.0))
		{
			Error = TEXT("River presentation requires riverMaterial, riverCellSize, riverSurfaceMargin and riverShoreWidth.");
			return false;
		}
		UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
		if (!Material)
		{
			Error = TEXT("Build the terrain art first (BuildTerrainArt.ps1): cannot load ") + MaterialPath;
			return false;
		}

		// One surface over the whole river, every channel and junction in it: a grid clipped to the water and a margin
		// beyond it, which the banks rise over. Its course comes only from World; it draws, never blocks (ADR-040 §6).
		const FVeyraRiverShape& Shape = VeyraRiver::ShapeOf(Tuning.Layout);
		const TArray<FVeyraRiverChannel>& Channels = Shape.GetChannels();
		FBox2D Bounds(ForceInit);
		for (const FVeyraRiverChannel& Channel : Channels)
		{
			for (const FVeyraRiverSample& Sample : Channel.Samples)
			{
				Bounds += Sample.Point - FVector2D(Sample.Width / 2.0 + Margin);
				Bounds += Sample.Point + FVector2D(Sample.Width / 2.0 + Margin);
			}
		}
		if (!Bounds.bIsValid)
		{
			Error = TEXT("The river has no channels.");
			return false;
		}
		// Each channel's downstream sense: the main channel's as authored, each side channel turned to agree with it.
		TArray<double> Sense;
		for (const FVeyraRiverChannel& Channel : Channels)
		{
			double Unused = 0.0;
			const FVector2D Main = FlowAlong(Channels[0], Channel.Samples[0].Point, Unused);
			const FVector2D Own = (Channel.Samples.Last().Point - Channel.Samples[0].Point).GetSafeNormal();
			Sense.Add(FVector2D::DotProduct(Main, Own) < 0.0 && &Channel != &Channels[0] ? -1.0 : 1.0);
		}

		const FIntPoint Count(FMath::CeilToInt(Bounds.GetSize().X / Cell) + 1, FMath::CeilToInt(Bounds.GetSize().Y / Cell) + 1);
		const double SurfaceZ = Tuning.Layout.River.SurfaceZ;
		FMeshDescription Mesh;
		FStaticMeshAttributes Attributes(Mesh);
		Attributes.Register();
		const FPolygonGroupID Group = Mesh.CreatePolygonGroup();
		Attributes.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Water");
		TVertexAttributesRef<FVector3f> Positions = Attributes.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> Normals = Attributes.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector3f> Tangents = Attributes.GetVertexInstanceTangents();
		TVertexInstanceAttributesRef<float> Signs = Attributes.GetVertexInstanceBinormalSigns();
		TVertexInstanceAttributesRef<FVector4f> Colors = Attributes.GetVertexInstanceColors();
		TVertexInstanceAttributesRef<FVector2f> UVs = Attributes.GetVertexInstanceUVs();
		TArray<FVertexInstanceID> Instances;
		TArray<double> Distances;
		Instances.SetNum(Count.X * Count.Y);
		Distances.SetNum(Count.X * Count.Y);
		for (int32 Y = 0; Y < Count.Y; ++Y)
		{
			for (int32 X = 0; X < Count.X; ++X)
			{
				const int32 Index = Y * Count.X + X;
				const FVector2D Point = Bounds.Min + FVector2D(X, Y) * Cell;
				Distances[Index] = Shape.SignedDistance(Point);
				Instances[Index] = FVertexInstanceID(INDEX_NONE);
				if (Distances[Index] > Margin + Cell)
				{
					continue;
				}
				// Downstream along the nearest channel, encoded 0..1 in red and green; how deep in the water, 0 at its
				// edge, in blue.
				FVector2D Flow = FVector2D::ZeroVector;
				double Nearest = TNumericLimits<double>::Max();
				for (int32 ChannelIndex = 0; ChannelIndex < Channels.Num(); ++ChannelIndex)
				{
					double Distance = 0.0;
					const FVector2D Along = FlowAlong(Channels[ChannelIndex], Point, Distance) * Sense[ChannelIndex];
					if (Distance < Nearest)
					{
						Nearest = Distance;
						Flow = Along;
					}
				}
				const FVertexID Vertex = Mesh.CreateVertex();
				Positions[Vertex] = FVector3f(Point.X, Point.Y, SurfaceZ);
				const FVertexInstanceID Instance = Mesh.CreateVertexInstance(Vertex);
				Normals[Instance] = FVector3f::UpVector;
				Tangents[Instance] = FVector3f::ForwardVector;
				Signs[Instance] = 1.0f;
				const double Deep = FMath::Clamp(-Distances[Index] / ShoreWidth, 0.0, 1.0);
				Colors[Instance] = FVector4f(Flow.X * 0.5f + 0.5f, Flow.Y * 0.5f + 0.5f, Deep, 1.0f);
				UVs[Instance] = FVector2f(Point.X / Cell, Point.Y / Cell);
				Instances[Index] = Instance;
			}
		}
		int32 Triangles = 0;
		for (int32 Y = 0; Y + 1 < Count.Y; ++Y)
		{
			for (int32 X = 0; X + 1 < Count.X; ++X)
			{
				const int32 Corners[4] = { Y * Count.X + X, Y * Count.X + X + 1, (Y + 1) * Count.X + X + 1, (Y + 1) * Count.X + X };
				bool bAllMade = true;
				bool bNearWater = false;
				for (const int32 Corner : Corners)
				{
					bAllMade &= Instances[Corner] != FVertexInstanceID(INDEX_NONE);
					bNearWater |= Distances[Corner] <= Margin;
				}
				if (!bAllMade || !bNearWater)
				{
					continue;
				}
				// Counter-clockwise seen from above, so the surface faces up.
				Mesh.CreateTriangle(Group, { Instances[Corners[0]], Instances[Corners[2]], Instances[Corners[1]] });
				Mesh.CreateTriangle(Group, { Instances[Corners[0]], Instances[Corners[3]], Instances[Corners[2]] });
				Triangles += 2;
			}
		}
		if (Triangles == 0)
		{
			Error = TEXT("The river's surface has no triangles.");
			return false;
		}

		UStaticMesh* Surface = NewObject<UStaticMesh>(World.GetPackage(), TEXT("SM_CrucibleRiver"), RF_Public | RF_Standalone);
		Surface->GetStaticMaterials().Add(FStaticMaterial(Material, TEXT("Water")));
		UStaticMesh::FBuildMeshDescriptionsParams Build;
		Build.bBuildSimpleCollision = false;
		Build.bFastBuild = true;
		Surface->BuildFromMeshDescriptions({ &Mesh }, Build);
		// Presentation has no collision at all: no simple shapes, and its triangles are not complex collision either.
		Surface->CreateBodySetup();
		Surface->GetBodySetup()->CollisionTraceFlag = CTF_UseSimpleAsComplex;
		Surface->bHasNavigationData = false;
		AStaticMeshActor* Actor = World.SpawnActor<AStaticMeshActor>();
		Actor->SetActorLabel(TEXT("Crucible_River"));
		Actor->Tags.Add(TEXT("Veyra.RiverPresentation"));
		UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
		Component->SetStaticMesh(Surface);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCanEverAffectNavigation(false);
		Component->SetCastShadow(false);
		Component->bAffectDistanceFieldLighting = false;
		Actor->SetMobility(EComponentMobility::Static);
		return true;
	}
}
