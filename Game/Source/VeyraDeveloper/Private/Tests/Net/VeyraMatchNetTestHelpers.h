// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "Engine/GameInstance.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Greybox/VeyraGreyboxLayout.h"
#include "HAL/IConsoleManager.h"
#include "Tuning/VeyraMatchTuningSubsystem.h"
#include "VeyraGameMode.h"
#include "VeyraGameState.h"
#include "VeyraPlayerController.h"
#include "VeyraVanguardCharacter.h"

namespace VeyraNetTests
{
	/** Match tuning a test may change. Get() returns it while this object lives. */
	struct FScopedMatchTuning
	{
		FVeyraMatchTuning Tuning;

		FScopedMatchTuning()
			: Tuning(UVeyraMatchTuningSubsystem::Get())
		{
			UVeyraMatchTuningSubsystem::SetTestOverride(&Tuning);
		}

		~FScopedMatchTuning()
		{
			UVeyraMatchTuningSubsystem::SetTestOverride(nullptr);
		}

		UE_NONCOPYABLE(FScopedMatchTuning);
	};

	/**
	 * Tells developer matches how many humans to wait for during loading, while this object lives.
	 * The variable is set by code directly: FScopedTestEnvironment cannot override a value its
	 * constructor set.
	 */
	struct FScopedExpectedPlayers
	{
		IConsoleVariable* Variable = nullptr;
		FString Previous;

		explicit FScopedExpectedPlayers(int32 Count)
			: Variable(IConsoleManager::Get().FindConsoleVariable(TEXT("veyra.Match.ExpectedPlayers")))
		{
			check(Variable);
			Previous = Variable->GetString();
			Variable->Set(*LexToString(Count), ECVF_SetByCode);
		}

		~FScopedExpectedPlayers()
		{
			Variable->Set(*Previous, ECVF_SetByCode);
		}

		UE_NONCOPYABLE(FScopedExpectedPlayers);
	};

	/** Humans in a developer test match: one for each side. */
	inline constexpr int32 MatchClientCount = 2;

	/** A developer match: a dedicated server running AVeyraGameMode and MatchClientCount clients. */
	template <typename StateType>
	void BuildMatchNetwork(FPIENetworkComponent<StateType>& Network)
	{
		FNetworkComponentBuilder<StateType>()
			.WithClients(MatchClientCount)
			.WithGameInstanceClass(UGameInstance::StaticClass())
			.WithGameMode(AVeyraGameMode::StaticClass())
			.Build(Network);
	}

	inline AVeyraGameMode* GameModeOf(const UWorld* World)
	{
		return World ? World->GetAuthGameMode<AVeyraGameMode>() : nullptr;
	}

	inline AVeyraGameState* GameStateOf(const UWorld* World)
	{
		return World ? World->GetGameState<AVeyraGameState>() : nullptr;
	}

	/** On a client: its own PlayerController. */
	inline AVeyraPlayerController* LocalControllerOf(const UWorld* World)
	{
		return World ? Cast<AVeyraPlayerController>(World->GetFirstPlayerController()) : nullptr;
	}

	/** On the server: the PlayerController of the client at ClientIndex. */
	inline AVeyraPlayerController* ServerControllerOf(const FBasePIENetworkComponentState& ServerState, int32 ClientIndex)
	{
		const UNetConnection* Connection = ServerState.ClientConnections.IsValidIndex(ClientIndex) ? ServerState.ClientConnections[ClientIndex] : nullptr;
		return Connection ? Cast<AVeyraPlayerController>(Connection->PlayerController) : nullptr;
	}

	/** The Vanguard of the participant with PlayerId, as this machine sees it. */
	inline AVeyraVanguardCharacter* FindVanguard(const UWorld* World, int32 PlayerId)
	{
		const AVeyraGameState* GameState = GameStateOf(World);
		if (!GameState)
		{
			return nullptr;
		}
		for (const APlayerState* Participant : GameState->PlayerArray)
		{
			if (Participant && Participant->GetPlayerId() == PlayerId)
			{
				return Cast<AVeyraVanguardCharacter>(Participant->GetPawn());
			}
		}
		return nullptr;
	}

	/**
	 * Builds the grey-box layout on every machine, then waits until the server reaches Phase and each
	 * client has its own Vanguard.
	 */
	template <typename StateType>
	FPIENetworkComponent<StateType>& StartMatch(FPIENetworkComponent<StateType>& Network, const FVeyraGreyboxLayout& Layout, EVeyraMatchPhase Phase)
	{
		return Network
			.ThenServer(TEXT("Build the map on the server"), [&Layout](StateType& State) {
				VeyraGreybox::SpawnFloor(*State.World, Layout, EComponentMobility::Movable);
				VeyraGreybox::SpawnTeamStarts(*State.World, Layout);
				VeyraGreybox::SpawnRuntimeNavigationBounds(*State.World, Layout);
			})
			.ThenClients(TEXT("Build the floor on each client"), [&Layout](StateType& State) {
				VeyraGreybox::SpawnFloor(*State.World, Layout, EComponentMobility::Movable);
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
