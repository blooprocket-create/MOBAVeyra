// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "GameFramework/Info.h"

#include "VeyraBattlegroundMarker.generated.h"

/**
 * Marks a map as the battleground (ADR-011 §12): a server whose map holds one spawns the structures
 * from World.json's layout as the match loads. The generated L_Battleground places it; nothing else
 * does, so the development grey box stays a plain lane. It carries no data and never replicates.
 */
UCLASS(NotBlueprintable)
class VEYRAWORLD_API AVeyraBattlegroundMarker : public AInfo
{
	GENERATED_BODY()

public:
	AVeyraBattlegroundMarker();

	/** Whether World's map is the battleground. */
	static bool IsBattleground(const UWorld& World);
};
