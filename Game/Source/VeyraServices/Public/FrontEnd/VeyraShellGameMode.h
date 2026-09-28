// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "GameFramework/GameModeBase.h"

#include "VeyraShellGameMode.generated.h"

/**
 * The game mode of the front end, L_FrontEnd (ADR-010 §3): the shell where a player signs in,
 * chooses how to play, picks a Vanguard and reads results. It spawns no pawn and runs no match; the
 * screens are presentation (VeyraUI) over the client-state coordinator. It lives here rather than in
 * VeyraUI so the map references nothing client-only, and the server cook stays clean.
 */
UCLASS()
class VEYRASERVICES_API AVeyraShellGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AVeyraShellGameMode();

protected:
	/** The shell's player has nothing to possess: it only gets its HUD. */
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
};
