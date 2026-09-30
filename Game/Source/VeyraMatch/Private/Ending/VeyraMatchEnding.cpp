// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Ending/VeyraMatchEnding.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Structures/VeyraStructure.h"
#include "VeyraGameState.h"

namespace VeyraMatchEnding
{
const AVeyraStructure* FindFallenPrimeWell(const UWorld& World)
{
	const AVeyraGameState* GameState = World.GetGameState<AVeyraGameState>();
	if (!GameState || GameState->GetMatchRules() != EVeyraMatchRules::Standard)
	{
		return nullptr;
	}
	for (TActorIterator<AVeyraStructure> It(const_cast<UWorld*>(&World)); It; ++It)
	{
		if (It->GetStructureKind() == EVeyraStructureKind::PrimeWell && It->IsDestroyed())
		{
			return *It;
		}
	}
	return nullptr;
}

EVeyraEndingHeadline Headline(const TOptional<EVeyraTeam>& FallenSide, EVeyraTeam Viewer)
{
	if (!FallenSide.IsSet() || (Viewer != EVeyraTeam::A && Viewer != EVeyraTeam::B))
	{
		return EVeyraEndingHeadline::MatchOver;
	}
	return FallenSide.GetValue() == Viewer ? EVeyraEndingHeadline::Defeat : EVeyraEndingHeadline::Victory;
}
}
