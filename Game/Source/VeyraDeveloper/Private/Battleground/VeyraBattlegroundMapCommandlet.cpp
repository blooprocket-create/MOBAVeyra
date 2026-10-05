// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Battleground/VeyraBattlegroundMapCommandlet.h"

#include "Battleground/VeyraBattlegroundBuilder.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"

#if WITH_EDITOR
#include "ActorFactories/ActorFactory.h"
#include "Features/IModularFeatures.h"
#include "Layout/VeyraWorldAuthoring.h"
#include "Battleground/VeyraBattlegroundMarker.h"
#include "Builders/CubeBuilder.h"
#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogVeyraBattlegroundMap, Log, All);

UVeyraBattlegroundMapCommandlet::UVeyraBattlegroundMapCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UVeyraBattlegroundMapCommandlet::Main(const FString& /*Params*/)
{
#if WITH_EDITOR
	// World.json loaded with the engine; a broken file has already failed the start-up. The production battleground's
	// ground is the authored terrain the generator builds, so the grey box's floor plays no part here.
	const FVeyraBattlegroundLayout& Layout = UVeyraWorldTuningSubsystem::Get().Layout;

	UPackage* Package = CreatePackage(MapPackageName);
	// This generator reconstructs every export; no previous map contents are retained.
	Package->MarkAsFullyLoaded();
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false, FName(FPackageName::GetShortName(MapPackageName)), Package);
	World->SetFlags(RF_Public | RF_Standalone);

	auto& Features = IModularFeatures::Get();
	FString GenerationError;
	if (!Features.IsModularFeatureAvailable(IVeyraWorldAuthoring::FeatureName())
		|| !Features.GetModularFeature<IVeyraWorldAuthoring>(IVeyraWorldAuthoring::FeatureName()).Generate(*World, GenerationError))
	{
		UE_LOG(LogVeyraBattlegroundMap, Error, TEXT("World authoring failed: %s"), *GenerationError);
		World->DestroyWorld(false);
		return 1;
	}
	if (!VeyraBattlegroundBuilder::SpawnTeamStarts(*World, Layout))
	{
		World->DestroyWorld(false);
		return 1;
	}
	World->SpawnActor<AVeyraBattlegroundMarker>();

	// Saved maps carry real brush geometry for their navigation bounds.
	ANavMeshBoundsVolume* Bounds = World->SpawnActor<ANavMeshBoundsVolume>();
	UCubeBuilder* Box = NewObject<UCubeBuilder>();
	const FVector BoundsSize(Layout.HalfExtent * 2.0, Layout.HalfExtent * 2.0, Layout.Surface.MaxZ - Layout.Surface.MinZ);
	Bounds->SetActorLocation(FVector(0.0, 0.0, (Layout.Surface.MinZ + Layout.Surface.MaxZ) / 2.0));
	Box->X = static_cast<float>(BoundsSize.X);
	Box->Y = static_cast<float>(BoundsSize.Y);
	Box->Z = static_cast<float>(BoundsSize.Z);
	UActorFactory::CreateBrushForVolumeActor(Bounds, Box);


	const FString Filename = FPackageName::LongPackageNameToFilename(MapPackageName, FPackageName::GetMapPackageExtension());
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), /*Tree*/ true);
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.Error = GError;
	const bool bSaved = UPackage::SavePackage(Package, World, *Filename, SaveArgs);
	World->DestroyWorld(/*bInformEngineOfWorld*/ false);

	UE_LOG(LogVeyraBattlegroundMap, Display, TEXT("%s %s."), bSaved ? TEXT("Saved") : TEXT("Failed to save"), *Filename);
	return bSaved ? 0 : 1;
#else
	UE_LOG(LogVeyraBattlegroundMap, Error, TEXT("The battleground map can only be built by the editor."));
	return 1;
#endif
}
