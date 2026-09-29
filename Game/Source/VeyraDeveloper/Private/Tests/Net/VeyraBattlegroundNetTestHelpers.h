// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "Battleground/VeyraBattlegroundBuilder.h"
#include "EngineUtils.h"
#include "Structures/VeyraStructure.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/World/VeyraBattlegroundTestLayout.h"
#include "VeyraBattlegroundSubsystem.h"

// Helpers the battleground's network suites share: a compact battleground built in every play
// session's worlds at runtime, since in-process play replicates no map-placed actor (ADR-006 §8).
namespace VeyraNetTests
{
	/** The structures this machine sees. */
	inline TArray<AVeyraStructure*> SeenStructures(const UWorld* World)
	{
		TArray<AVeyraStructure*> Structures;
		for (TActorIterator<AVeyraStructure> It(World); It; ++It)
		{
			Structures.Add(*It);
		}
		return Structures;
	}

	/**
	 * Builds the compact battleground on every machine and spawns its structures on the server, then
	 * waits until the server reaches Phase and each client has its own Vanguard.
	 */
	template <typename StateType>
	FPIENetworkComponent<StateType>& StartBattleground(FPIENetworkComponent<StateType>& Network, const FVeyraGreyboxLayout& Greybox, EVeyraMatchPhase Phase)
	{
		return Network
			.ThenServer(TEXT("Build the battleground on the server"), [&Greybox](StateType& State) {
				const FVeyraBattlegroundLayout Layout = VeyraWorldTests::CompactBattleground();
				VeyraBattlegroundBuilder::SpawnRuntimeBattleground(*State.World, Layout, Greybox, /*bServer*/ true);
				State.World->template GetSubsystem<UVeyraBattlegroundSubsystem>()->SpawnStructures(Layout);
			})
			.ThenClients(TEXT("Build the floor on each client"), [&Greybox](StateType& State) {
				VeyraBattlegroundBuilder::SpawnRuntimeBattleground(*State.World, VeyraWorldTests::CompactBattleground(), Greybox, /*bServer*/ false);
			})
			.UntilServer(TEXT("Reach the phase"), [Phase](StateType& State) {
				const AVeyraGameState* GameState = GameStateOf(State.World);
				return GameState && GameState->GetPhase() >= Phase;
			})
			.UntilClients(TEXT("Each client has its Vanguard"), [Phase](StateType& State) {
				const AVeyraGameState* GameState = GameStateOf(State.World);
				const AVeyraPlayerController* Controller = LocalControllerOf(State.World);
				return GameState && GameState->GetPhase() >= Phase && Controller && Controller->GetVanguard();
			});
	}
}

#endif // ENABLE_PIE_NETWORK_TEST
