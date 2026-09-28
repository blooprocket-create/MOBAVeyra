// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "FrontEnd/VeyraFrontEndMapCommandlet.h"

#include "FrontEnd/VeyraShellGameMode.h"

#if WITH_EDITOR
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogVeyraFrontEndMap, Log, All);

UVeyraFrontEndMapCommandlet::UVeyraFrontEndMapCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UVeyraFrontEndMapCommandlet::Main(const FString& /*Params*/)
{
#if WITH_EDITOR
	UPackage* Package = CreatePackage(MapPackageName);
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false, FName(FPackageName::GetShortName(MapPackageName)), Package);
	World->SetFlags(RF_Public | RF_Standalone);
	World->GetWorldSettings()->DefaultGameMode = AVeyraShellGameMode::StaticClass();

	const FString Filename = FPackageName::LongPackageNameToFilename(MapPackageName, FPackageName::GetMapPackageExtension());
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), /*Tree*/ true);
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.Error = GError;
	const bool bSaved = UPackage::SavePackage(Package, World, *Filename, SaveArgs);
	World->DestroyWorld(/*bInformEngineOfWorld*/ false);

	UE_LOG(LogVeyraFrontEndMap, Display, TEXT("%s %s."), bSaved ? TEXT("Saved") : TEXT("Failed to save"), *Filename);
	return bSaved ? 0 : 1;
#else
	UE_LOG(LogVeyraFrontEndMap, Error, TEXT("The front-end map can only be built by the editor."));
	return 1;
#endif
}
