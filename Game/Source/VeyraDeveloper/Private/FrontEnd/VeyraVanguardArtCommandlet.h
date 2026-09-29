// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Commandlets/Commandlet.h"

#include "VeyraVanguardArtCommandlet.generated.h"

/**
 * Saves each playable Vanguard's champion-select art: every <id>.png in the -Source folder becomes the
 * UI texture VeyraShellArt::HeroPackageName(<id>), replacing it. Game/Scripts/BuildVanguardArt.ps1
 * converts the hero illustrations in ConceptArt/Vanguards to those PNGs and runs it, so the art stays
 * reproducible from its source like the maps. It needs the editor; other builds report an error.
 */
UCLASS()
class UVeyraVanguardArtCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UVeyraVanguardArtCommandlet();

	virtual int32 Main(const FString& Params) override;
};
