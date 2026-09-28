// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Commandlets/Commandlet.h"

#include "VeyraFrontEndMapCommandlet.generated.h"

/**
 * Saves the front-end map, L_FrontEnd (ADR-010 §3): an empty world whose game mode is
 * AVeyraShellGameMode, where the shell's screens are drawn. Generated like the grey-box map, so it
 * stays reproducible from reviewed code. Run it with Game/Scripts/BuildFrontEndMap.ps1. It needs the
 * editor; other builds report an error.
 */
UCLASS()
class UVeyraFrontEndMapCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UVeyraFrontEndMapCommandlet();

	/** The map this commandlet writes, which is the game's default map. */
	static constexpr const TCHAR* MapPackageName = TEXT("/Game/Veyra/FrontEnd/Maps/L_FrontEnd");

	virtual int32 Main(const FString& Params) override;
};
