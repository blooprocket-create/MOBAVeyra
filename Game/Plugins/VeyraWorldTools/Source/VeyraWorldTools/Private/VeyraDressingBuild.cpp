// Copyright © 2026 Wayfinder Studios. All rights reserved.
#include "VeyraWorldBuild.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Elements/PCGCreatePoints.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "Layout/VeyraLayout.h"
#include "Layout/VeyraDressingRules.h"
#include "Layout/VeyraTerrainField.h"
#include "MeshSelectors/PCGMeshSelectorWeighted.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGSubsystem.h"
#include "Tuning/VeyraWorldTuning.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"

namespace VeyraWorldBuild
{
namespace
{
bool Bake(UWorld& World, const FString& Region, const FString& Family, const TArray<FTransform>& Transforms, int32 Seed, double Timeout, FString& Error)
{
    if (Transforms.IsEmpty()) { return true; }
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *(TEXT("/Game/Veyra/World/Environment/Meshes/SM_Crucible_") + Family + TEXT("_00")));
    if (!Mesh) { Error = TEXT("Import the environment kit before generation: ") + Family; return false; }
    AActor* Source = World.SpawnActor<AActor>();
    FBox Bounds(ForceInit);
    for (const auto& Transform : Transforms) { Bounds += Transform.GetLocation(); }
    Bounds = Bounds.ExpandBy(Mesh->GetBounds().SphereRadius);
    UBoxComponent* Root = NewObject<UBoxComponent>(Source);
    Root->SetBoxExtent(Bounds.GetExtent());
    Root->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Root->SetCanEverAffectNavigation(false);
    Source->SetRootComponent(Root);
    Source->AddInstanceComponent(Root);
    Root->RegisterComponent();
    Source->SetActorLocation(Bounds.GetCenter());
    UPCGComponent* PCG = NewObject<UPCGComponent>(Source);
    Source->AddInstanceComponent(PCG);
    PCG->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
    PCG->Seed = Seed;
    PCG->RegisterComponent();
    UPCGGraph* Graph = NewObject<UPCGGraph>(Source);
    UPCGCreatePointsSettings* Points = nullptr;
    UPCGNode* Input = Graph->AddNodeOfType(Points);
    Points->CoordinateSpace = EPCGCoordinateSpace::World;
    Points->PointsToCreate.Reset();
    for (int32 I = 0; I < Transforms.Num(); ++I)
    {
        FPCGPoint& Point = Points->PointsToCreate.AddDefaulted_GetRef();
        Point.Transform = Transforms[I];
        Point.Seed = Seed + I;
        Point.Density = 1.0f;
    }
    UPCGStaticMeshSpawnerSettings* Spawner = nullptr;
    UPCGNode* Output = Graph->AddNodeOfType(Spawner);
    Spawner->SetMeshSelectorType(UPCGMeshSelectorWeighted::StaticClass());
    Spawner->bSynchronousLoad = true;
    auto* Selector = CastChecked<UPCGMeshSelectorWeighted>(Spawner->MeshSelectorParameters);
    auto& Entry = Selector->MeshEntries.Emplace_GetRef(TSoftObjectPtr<UStaticMesh>(Mesh), 1);
    Entry.Descriptor.BodyInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Entry.Descriptor.bCanEverAffectNavigation = false;
    Graph->AddEdge(Input, PCGPinConstants::DefaultOutputLabel, Output, PCGPinConstants::DefaultInputLabel);
    PCG->SetGraph(Graph);
    UPCGSubsystem* Subsystem = UPCGSubsystem::GetInstance(&World);
    if (!Subsystem) { Error = TEXT("PCG subsystem unavailable in authoring world."); return false; }
    PCG->GenerateLocal(true);
    const double Deadline = FPlatformTime::Seconds() + Timeout;
    while (PCG->IsGenerating() && FPlatformTime::Seconds() < Deadline)
    {
        Subsystem->Tick(0.0f);
        FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
        FPlatformProcess::SleepNoStats(0.001f);
    }
    if (PCG->IsGenerating()) { Error = TEXT("PCG generation timed out for ") + Region; return false; }
    AActor* Baked = PCG->ClearPCGLink();
    if (!Baked) { Error = TEXT("PCG generated no baked resources for ") + Region; return false; }
    Baked->SetActorLabel(TEXT("Crucible_") + Region);
    Baked->Tags.Add(TEXT("Veyra.GeneratedDressing"));
    TArray<UInstancedStaticMeshComponent*> Instances;
    Baked->GetComponents(Instances);
    int32 Count = 0;
    for (auto* Component : Instances)
    {
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetCanEverAffectNavigation(false);
        Count += Component->GetInstanceCount();
    }
    World.DestroyActor(Source);
    if (Count != Transforms.Num()) { Error = FString::Printf(TEXT("PCG %s baked %d of %d points."), *Region, Count, Transforms.Num()); return false; }
    FString Canonical;
    for (const auto& Transform : Transforms)
    {
        const auto P = Transform.GetLocation();
        const auto Q = Transform.GetRotation();
        const auto S = Transform.GetScale3D();
        Canonical += FString::Printf(TEXT("%.9f,%.9f,%.9f;%.9f,%.9f,%.9f,%.9f;%.9f,%.9f,%.9f\n"), P.X, P.Y, P.Z, Q.X, Q.Y, Q.Z, Q.W, S.X, S.Y, S.Z);
    }
    auto Manifest = MakeShared<FJsonObject>();
    Manifest->SetStringField(TEXT("region"), Region);
    Manifest->SetStringField(TEXT("generator"), TEXT("VeyraWorldTools.v1"));
    Manifest->SetNumberField(TEXT("seed"), Seed);
    Manifest->SetNumberField(TEXT("instanceCount"), Count);
    Manifest->SetStringField(TEXT("mesh"), Mesh->GetPathName());
    Manifest->SetStringField(TEXT("placementDigest"), FMD5::HashAnsiString(*Canonical));
    Manifest->SetStringField(TEXT("collision"), TEXT("None; World owns terrain collision"));
    FString Serialized;
    FJsonSerializer::Serialize(Manifest, TJsonWriterFactory<>::Create(&Serialized));
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("WorldGeneration/Regions");
    IFileManager::Get().MakeDirectory(*Directory, true);
    if (!FFileHelper::SaveStringToFile(Serialized, *(Directory / (Region + TEXT(".json"))), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        Error = TEXT("Cannot write region generation manifest: ") + Region;
        return false;
    }
    return true;
}
}

bool Dressing(UWorld& World, const FVeyraWorldTuning& Tuning, const FJsonObject& Style, FString& Error)
{
    const auto& Layout = Tuning.Layout;
    const FVeyraTerrainField Terrain(Tuning, ReliefOf(Style));
    const int32 Seed = static_cast<int32>(Style.GetNumberField(TEXT("seed")));
    const double Timeout = Style.GetNumberField(TEXT("pcgTimeoutSeconds"));
    const double Spacing = Style.GetNumberField(TEXT("wallMeshSpacing"));
    const double Overlap = Style.GetNumberField(TEXT("wallMeshOverlap"));
    UStaticMesh* Shelf = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Veyra/World/Environment/Meshes/SM_Crucible_Shelf_00"));
    if (!Shelf || Spacing <= 0.0 || Overlap < 1.0) { Error = TEXT("Missing shelf mesh or invalid wall dressing profile."); return false; }
    const FVector MeshSize = Shelf->GetBoundingBox().GetSize();
    TArray<FTransform> WallPoints;
    for (const auto& Wall : VeyraLayout::Walls(Layout))
    {
        const FVector2D Along(-Wall.Facing.Y, Wall.Facing.X);
        const int32 Count = FMath::Max(1, FMath::CeilToInt(Wall.Length / Spacing));
        const double Step = Wall.Length / Count;
        for (int32 I = 0; I < Count; ++I)
        {
            const FVector2D Point = Wall.Centre + Along * ((I + 0.5) * Step - Wall.Length / 2.0);
            const FVector Scale(Step * Overlap / MeshSize.X, (Wall.Thickness + Layout.Terrain.WallFootingClearance * 2.0) / MeshSize.Y, Layout.WallHalfHeight * 2.0 / MeshSize.Z);
            WallPoints.Add(FTransform(FRotator(0.0, FMath::RadiansToDegrees(FMath::Atan2(Along.Y, Along.X)), 0.0), FVector(Point, Terrain.Height(Point) - Layout.Terrain.WallFootingClearance), Scale));
        }
    }
    if (!Bake(World, TEXT("Macro_WallShelves"), TEXT("Shelf"), WallPoints, Seed, Timeout, Error)) { return false; }

    // Each scale is independently seeded and regenerated. Rotated pairs keep both teams' silhouettes the same.
    const TArray<FString> Scales = { TEXT("macro"), TEXT("medium"), TEXT("micro") };
    for (int32 Level = 0; Level < Scales.Num(); ++Level)
    {
        const FString& ScaleName = Scales[Level];
        FRandomStream Random(Seed + Level);
        const double Grid = Style.GetNumberField(ScaleName + TEXT("Spacing"));
        const double Density = Style.GetNumberField(ScaleName + TEXT("Density"));
        const double Jitter = Style.GetNumberField(TEXT("pointJitterFraction"));
        if (Grid <= 0.0 || Density < 0.0 || Density > 1.0 || Jitter < 0.0 || Jitter >= 0.5) { Error = TEXT("Invalid dressing density/grid."); return false; }
        const double Extent = Layout.HalfExtent + Layout.Terrain.BoundaryWidth;
        const FString Family = Level == 0 ? TEXT("Cliff") : Level == 1 ? TEXT("Shrub") : TEXT("Riverstone");
        UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *(TEXT("/Game/Veyra/World/Environment/Meshes/SM_Crucible_") + Family + TEXT("_00")));
        if (!Mesh) { Error = TEXT("Missing dressing mesh: ") + Family; return false; }
        TArray<FTransform> Points;
        const double MeshScale = Level == 2 ? Style.GetNumberField(TEXT("microMeshScale")) : 1.0;
        const double Radius = Mesh->GetBounds().SphereRadius * MeshScale;
        for (double X = -Extent + Grid; X < Extent; X += Grid)
        {
            for (double Y = -Extent + Grid; Y < Extent; Y += Grid)
            {
                const FVector2D Point(X + Random.FRandRange(-Jitter, Jitter) * Grid, Y + Random.FRandRange(-Jitter, Jitter) * Grid);
                if (Point.X + Point.Y >= -Radius * 2.0 || Random.FRand() > Density) { continue; }
                const bool bExterior = FMath::Max(FMath::Abs(Point.X), FMath::Abs(Point.Y)) > Layout.HalfExtent + Radius;
                if (Level == 0 && !bExterior) { continue; }
                if (!bExterior && !VeyraDressing::Allows(Tuning, Point, Radius)) { continue; }
                bool bWall = false;
                for (const auto& Wall : VeyraLayout::Walls(Layout)) { bWall |= Wall.DistanceTo(Point) < Radius; }
                if (bWall) { continue; }
                const double Yaw = Random.FRandRange(0.0, 360.0);
                for (const bool bRotated : { false, true })
                {
                    const FVector2D Position = bRotated ? VeyraLayout::Rotate(Point) : Point;
                    Points.Add(FTransform(FRotator(0.0, bRotated ? Yaw + 180.0 : Yaw, 0.0), FVector(Position, Terrain.Height(Position)), FVector(MeshScale)));
                }
            }
        }
        if (!Bake(World, ScaleName + TEXT("_Dressing"), Family, Points, Seed + Level, Timeout, Error)) { return false; }
    }
    return true;
}
}
