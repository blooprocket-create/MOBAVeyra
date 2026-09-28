// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "FrontEnd/VeyraShellGameMode.h"

#include "GameFramework/PlayerController.h"

AVeyraShellGameMode::AVeyraShellGameMode()
{
	DefaultPawnClass = nullptr;
	bStartPlayersAsSpectators = false;
}

void AVeyraShellGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	if (NewPlayer)
	{
		InitializeHUDForPlayer(NewPlayer);
	}
}
