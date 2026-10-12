// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Greybox/VeyraGreyboxMapCommandlet.h"

#include "Greybox/VeyraGreyboxLayout.h"

#if WITH_EDITOR
#include "ActorFactories/ActorFactory.h"
#include "Builders/CubeBuilder.h"
#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogVeyraGreybox, Log, All);

UVeyraGreyboxMapCommandlet::UVeyraGreyboxMapCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UVeyraGreyboxMapCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	FString LayoutFile = VeyraGreybox::TestMapLayoutFile;
	FString MapPackage = MapPackageName;
	FParse::Value(*Params, TEXT("Layout="), LayoutFile);
	FParse::Value(*Params, TEXT("Map="), MapPackage);
	// A layout is a file beside the test map's, and a map is a developer map.
	if (LayoutFile.IsEmpty() || LayoutFile != FPaths::GetCleanFilename(LayoutFile) || !LayoutFile.EndsWith(TEXT(".json"))
		|| !MapPackage.StartsWith(MapFolder) || !FPackageName::IsValidLongPackageName(MapPackage))
	{
		UE_LOG(LogVeyraGreybox, Error, TEXT("-Layout= names a .json file in Source/VeyraDeveloper/Greybox/, and -Map= a package under %s."), MapFolder);
		return 1;
	}
	FVeyraGreyboxLayout Layout;
	const VeyraTuning::FErrors Errors = VeyraGreybox::LoadLayout(Layout, *LayoutFile);
	for (const FString& Error : Errors)
	{
		UE_LOG(LogVeyraGreybox, Error, TEXT("%s: %s"), *LayoutFile, *Error);
	}
	if (!Errors.IsEmpty())
	{
		return 1;
	}

	UPackage* Package = CreatePackage(*MapPackage);
	// Built whole here, so it saves over the map already on disk rather than refusing it as partly loaded.
	Package->MarkAsFullyLoaded();
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false, FName(FPackageName::GetShortName(MapPackage)), Package);
	World->SetFlags(RF_Public | RF_Standalone);

	VeyraGreybox::SpawnFloor(*World, Layout, EComponentMobility::Static);
	VeyraGreybox::SpawnTeamStarts(*World, Layout);

	// Saved maps carry real brush geometry for their navigation bounds.
	ANavMeshBoundsVolume* Bounds = World->SpawnActor<ANavMeshBoundsVolume>();
	UCubeBuilder* Box = NewObject<UCubeBuilder>();
	const FVector BoundsSize = VeyraGreybox::NavigationBoundsSize(Layout);
	Box->X = static_cast<float>(BoundsSize.X);
	Box->Y = static_cast<float>(BoundsSize.Y);
	Box->Z = static_cast<float>(BoundsSize.Z);
	UActorFactory::CreateBrushForVolumeActor(Bounds, Box);

	// A movable light, so the map needs no light build.
	ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(Layout.Sun.PitchDegrees, Layout.Sun.YawDegrees, 0.0));
	Sun->GetRootComponent()->SetMobility(EComponentMobility::Movable);

	const FString Filename = FPackageName::LongPackageNameToFilename(MapPackage, FPackageName::GetMapPackageExtension());
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), /*Tree*/ true);
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.Error = GError;
	const bool bSaved = UPackage::SavePackage(Package, World, *Filename, SaveArgs);
	World->DestroyWorld(/*bInformEngineOfWorld*/ false);

	UE_LOG(LogVeyraGreybox, Display, TEXT("%s %s."), bSaved ? TEXT("Saved") : TEXT("Failed to save"), *Filename);
	return bSaved ? 0 : 1;
#else
	UE_LOG(LogVeyraGreybox, Error, TEXT("The grey-box map can only be built by the editor."));
	return 1;
#endif
}
