// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Commandlets/Commandlet.h"

#include "VeyraGreyboxMapCommandlet.generated.h"

/**
 * Saves the grey-box test map from Source/VeyraDeveloper/Greybox/Greybox.json, so the map stays
 * reproducible from reviewed text (AGENTS.md: document how an asset-side change is made). Run it
 * with Game/Scripts/BuildGreyboxMap.ps1. It needs the editor; other builds report an error.
 */
UCLASS()
class UVeyraGreyboxMapCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UVeyraGreyboxMapCommandlet();

	/** The map this commandlet writes. */
	static constexpr const TCHAR* MapPackageName = TEXT("/Game/Veyra/Developer/Maps/L_Greybox");

	virtual int32 Main(const FString& Params) override;
};
