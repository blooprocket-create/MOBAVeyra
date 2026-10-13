// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Battleground/VeyraBattlegroundMarker.h"

#include "EngineUtils.h"

AVeyraBattlegroundMarker::AVeyraBattlegroundMarker()
{
	// Every machine finds it: the server spawns the structures, and each machine raises its own terrain (ADR-073 §3).
	bReplicates = false;
	bNetLoadOnClient = true;
}

bool AVeyraBattlegroundMarker::IsBattleground(const UWorld& World)
{
	return static_cast<bool>(TActorIterator<AVeyraBattlegroundMarker>(&World));
}
