// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Battleground/VeyraBattlegroundMarker.h"

#include "EngineUtils.h"

AVeyraBattlegroundMarker::AVeyraBattlegroundMarker()
{
	bReplicates = false;
	bNetLoadOnClient = false;
}

bool AVeyraBattlegroundMarker::IsBattleground(const UWorld& World)
{
	return static_cast<bool>(TActorIterator<AVeyraBattlegroundMarker>(&World));
}
