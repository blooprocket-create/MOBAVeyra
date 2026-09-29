// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Battleground/VeyraBattlegroundMapCommandlet.h"

#include "Battleground/VeyraBattlegroundBuilder.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"

#if WITH_EDITOR
#include "ActorFactories/ActorFactory.h"
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
	FVeyraGreyboxLayout Greybox;
	const VeyraTuning::FErrors Errors = VeyraGreybox::LoadLayout(Greybox);
	for (const FString& Error : Errors)
	{
		UE_LOG(LogVeyraBattlegroundMap, Error, TEXT("Greybox.json: %s"), *Error);
	}
	if (!Errors.IsEmpty())
	{
		return 1;
	}
	// World.json loaded with the engine; a broken file has already failed the start-up.
	const FVeyraBattlegroundLayout& Layout = UVeyraWorldTuningSubsystem::Get().Layout;
	const FVeyraGreyboxLayout Floor = VeyraBattlegroundBuilder::AsGreybox(Layout, Greybox);

	UPackage* Package = CreatePackage(MapPackageName);
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false, FName(FPackageName::GetShortName(MapPackageName)), Package);
	World->SetFlags(RF_Public | RF_Standalone);

	VeyraGreybox::SpawnFloor(*World, Floor, EComponentMobility::Static);
	VeyraBattlegroundBuilder::SpawnTeamStarts(*World, Layout);
	World->SpawnActor<AVeyraBattlegroundMarker>();

	// Saved maps carry real brush geometry for their navigation bounds.
	ANavMeshBoundsVolume* Bounds = World->SpawnActor<ANavMeshBoundsVolume>();
	UCubeBuilder* Box = NewObject<UCubeBuilder>();
	const FVector BoundsSize = VeyraGreybox::NavigationBoundsSize(Floor);
	Box->X = static_cast<float>(BoundsSize.X);
	Box->Y = static_cast<float>(BoundsSize.Y);
	Box->Z = static_cast<float>(BoundsSize.Z);
	UActorFactory::CreateBrushForVolumeActor(Bounds, Box);

	// A movable light, so the map needs no light build.
	ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(Greybox.Sun.PitchDegrees, Greybox.Sun.YawDegrees, 0.0));
	Sun->GetRootComponent()->SetMobility(EComponentMobility::Movable);

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
