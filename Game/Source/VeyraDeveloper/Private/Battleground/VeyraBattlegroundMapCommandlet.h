// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Commandlets/Commandlet.h"

#include "VeyraBattlegroundMapCommandlet.generated.h"

/**
 * Saves the battleground map, L_Battleground, from Game/Tuning/World.json's layout (ADR-011 §12), so
 * the map stays reproducible from reviewed text: the floor, each team's start at its fountain,
 * navigation bounds, a sun, and the marker that tells the server to spawn the structures. Run it
 * with Game/Scripts/BuildBattlegroundMap.ps1. It needs the editor; other builds report an error.
 */
UCLASS()
class UVeyraBattlegroundMapCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UVeyraBattlegroundMapCommandlet();

	/** The map this commandlet writes. */
	static constexpr const TCHAR* MapPackageName = TEXT("/Game/Veyra/World/Maps/L_Battleground");

	virtual int32 Main(const FString& Params) override;
};
