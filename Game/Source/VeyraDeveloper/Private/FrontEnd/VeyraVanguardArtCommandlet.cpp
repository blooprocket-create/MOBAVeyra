// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "FrontEnd/VeyraVanguardArtCommandlet.h"

#if WITH_EDITOR && WITH_VEYRA_UI
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "IImageWrapperModule.h"
#include "ImageCore.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Shell/VeyraShellArt.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogVeyraVanguardArt, Log, All);

UVeyraVanguardArtCommandlet::UVeyraVanguardArtCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UVeyraVanguardArtCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR && WITH_VEYRA_UI
	FString Source;
	if (!FParse::Value(*Params, TEXT("Source="), Source) || !IFileManager::Get().DirectoryExists(*Source))
	{
		UE_LOG(LogVeyraVanguardArt, Error, TEXT("Pass -Source=<folder of <vanguard id>.png files>, as BuildVanguardArt.ps1 does."));
		return 1;
	}
	// Vanguards' hero art, or with -Kind=Items the items' icons: the same kind of UI texture.
	FString Kind;
	FParse::Value(*Params, TEXT("Kind="), Kind);
	const bool bItems = Kind == TEXT("Items");
	const bool bAbilities = Kind == TEXT("Abilities");
	if (!Kind.IsEmpty() && !bItems && !bAbilities && Kind != TEXT("Vanguards"))
	{
		UE_LOG(LogVeyraVanguardArt, Error, TEXT("-Kind is Vanguards, Items or Abilities, not %s."), *Kind);
		return 1;
	}
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(Source, TEXT("*.png")), /*Files*/ true, /*Directories*/ false);
	Files.Sort();
	if (Files.IsEmpty())
	{
		UE_LOG(LogVeyraVanguardArt, Error, TEXT("%s holds no PNG."), *Source);
		return 1;
	}

	IImageWrapperModule& Images = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	int32 Failures = 0;
	for (const FString& File : Files)
	{
		const FString Id = FPaths::GetBaseFilename(File);
		TArray<uint8> Bytes;
		FImage Image;
		if (!FFileHelper::LoadFileToArray(Bytes, *FPaths::Combine(Source, File)) || !Images.DecompressImage(Bytes.GetData(), Bytes.Num(), Image))
		{
			UE_LOG(LogVeyraVanguardArt, Error, TEXT("Could not read %s."), *File);
			++Failures;
			continue;
		}
		Image.ChangeFormat(ERawImageFormat::BGRA8, EGammaSpace::sRGB);

		const FString PackageName = bItems ? VeyraShellArt::ItemIconPackageName(Id)
			: bAbilities				   ? VeyraShellArt::AbilityIconPackageName(Id)
										   : VeyraShellArt::HeroPackageName(Id);
		// An existing texture is loaded whole and baked again in place; a package only partly loaded
		// cannot be saved.
		UPackage* Package = CreatePackage(*PackageName);
		Package->FullyLoad();
		const FName TextureName(FPackageName::GetShortName(PackageName));
		UTexture2D* Texture = FindObject<UTexture2D>(Package, *TextureName.ToString());
		if (!Texture)
		{
			Texture = NewObject<UTexture2D>(Package, TextureName, RF_Public | RF_Standalone);
		}
		Texture->Source.Init(Image.SizeX, Image.SizeY, /*NumSlices*/ 1, /*NumMips*/ 1, TSF_BGRA8, Image.RawData.GetData());
		// Screen art: shown whole at once, so no mips and no streaming, in the UI's texture group.
		Texture->SRGB = true;
		Texture->CompressionSettings = TC_BC7;
		Texture->LODGroup = TEXTUREGROUP_UI;
		Texture->MipGenSettings = TMGS_NoMipmaps;
		Texture->NeverStream = true;
		Texture->PostEditChange();

		const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), /*Tree*/ true);
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.Error = GError;
		if (UPackage::SavePackage(Package, Texture, *Filename, SaveArgs))
		{
			UE_LOG(LogVeyraVanguardArt, Display, TEXT("Saved %s (%d x %d)."), *Filename, Image.SizeX, Image.SizeY);
		}
		else
		{
			UE_LOG(LogVeyraVanguardArt, Error, TEXT("Failed to save %s."), *Filename);
			++Failures;
		}
	}
	return Failures == 0 ? 0 : 1;
#else
	UE_LOG(LogVeyraVanguardArt, Error, TEXT("The Vanguard art can only be built by the editor."));
	return 1;
#endif
}
