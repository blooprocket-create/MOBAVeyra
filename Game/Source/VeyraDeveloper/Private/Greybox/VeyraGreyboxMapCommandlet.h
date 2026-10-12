// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Commandlets/Commandlet.h"

#include "VeyraGreyboxMapCommandlet.generated.h"

/**
 * Saves a grey-box map from its layout in Source/VeyraDeveloper/Greybox/, so the map stays reproducible from reviewed
 * text (AGENTS.md: document how an asset-side change is made). With no arguments it writes the test map from
 * Greybox.json; -Layout=<file> -Map=<package> writes another, as the skill showcase's stage (ADR-074 §1). Run it with
 * Game/Scripts/BuildGreyboxMap.ps1. It needs the editor; other builds report an error.
 */
UCLASS()
class UVeyraGreyboxMapCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UVeyraGreyboxMapCommandlet();

	/** The map this commandlet writes unless told another. */
	static constexpr const TCHAR* MapPackageName = TEXT("/Game/Veyra/Developer/Maps/L_Greybox");

	/** Where every map it writes lives: developer maps only. */
	static constexpr const TCHAR* MapFolder = TEXT("/Game/Veyra/Developer/Maps/");

	virtual int32 Main(const FString& Params) override;
};
