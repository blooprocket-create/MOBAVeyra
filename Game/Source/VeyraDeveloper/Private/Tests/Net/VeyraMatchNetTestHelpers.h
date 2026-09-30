// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Greybox/VeyraGreyboxLayout.h"
#include "HAL/IConsoleManager.h"
#include "Hash/VeyraSha256.h"
#include "Join/VeyraMatchHostSubsystem.h"
#include "Rules/VeyraMatchRules.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraMatchTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "Tuning/VeyraVisionTuningSubsystem.h"
#include "VeyraGameMode.h"
#include "VeyraGameState.h"
#include "VeyraLocalPlayer.h"
#include "VeyraPlayerController.h"
#include "VeyraVanguardCharacter.h"

namespace VeyraNetTests
{
	/** The developer test Vanguard, whose Q is the developer test ability (Vanguards.json). */
	inline FVeyraContentId TestVanguardId()
	{
		return FVeyraContentId::FromText(TEXT("test_vanguard")).GetValue();
	}

	inline const FVeyraVanguardDefinition& TestVanguard()
	{
		return *UVeyraVanguardsTuningSubsystem::FindVanguard(TestVanguardId());
	}

	/** The ability in the test Vanguard's Q slot. */
	inline FVeyraContentId TestVanguardAbilityQ()
	{
		return TestVanguard().Abilities.Q[0];
	}

	/**
	 * Match tuning a test may change. Get() returns it while this object lives. Every participant
	 * plays the test Vanguard, with Q learned at level 1, and recovers nothing at its fountain, where
	 * it starts, unless the test chooses otherwise.
	 */
	/**
	 * Vision tuning a test may change, starting from the committed one; Get() returns it while this
	 * object lives. Committed keeps the file's values for a test that restores them.
	 */
	struct FScopedVisionTuning
	{
		const FVeyraVisionTuning Committed;
		FVeyraVisionTuning Tuning;

		FScopedVisionTuning()
			: Committed(UVeyraVisionTuningSubsystem::Get())
			, Tuning(Committed)
		{
			UVeyraVisionTuningSubsystem::SetTestOverride(&Tuning);
		}

		~FScopedVisionTuning()
		{
			UVeyraVisionTuningSubsystem::SetTestOverride(nullptr);
		}

		/** Every unit sees across any test map: for tests that are not about fog (ADR-016). */
		void SeeEverything()
		{
			// Far beyond any map a test builds; a fixture value, not tuning.
			constexpr double AcrossAnyTestMap = 1.0e7;
			Tuning.Sight.Vanguard = AcrossAnyTestMap;
			Tuning.Sight.Fluxborn = AcrossAnyTestMap;
			Tuning.Sight.Structure = AcrossAnyTestMap;
		}

		UE_NONCOPYABLE(FScopedVisionTuning);
	};

	struct FScopedMatchTuning
	{
		FVeyraMatchTuning Tuning;
		/**
		 * A match's vision, which sees everything so a test that is not about fog keeps every client
		 * receiving every unit; a fog test restores Vision.Committed.
		 */
		FScopedVisionTuning Vision;

		FScopedMatchTuning()
			: Tuning(UVeyraMatchTuningSubsystem::Get())
		{
			Vision.SeeEverything();
			Tuning.DeveloperMatch.Vanguards = { TestVanguardId() };
			Tuning.DeveloperMatch.StartingRank = EVeyraDeveloperStartingRank::Q;
			Tuning.Fountain.HealthFractionPerSecond = 0.0;
			Tuning.Fountain.ResourceFractionPerSecond = 0.0;
			UVeyraMatchTuningSubsystem::SetTestOverride(&Tuning);
		}

		~FScopedMatchTuning()
		{
			UVeyraMatchTuningSubsystem::SetTestOverride(nullptr);
		}

		UE_NONCOPYABLE(FScopedMatchTuning);
	};

	/** Vanguards tuning a test may change, starting from the committed one. Get() returns it while this object lives. */
	struct FScopedVanguardsTuning
	{
		FVeyraVanguardsTuning Tuning;

		FScopedVanguardsTuning()
			: Tuning(UVeyraVanguardsTuningSubsystem::Get())
		{
			UVeyraVanguardsTuningSubsystem::SetTestOverride(&Tuning);
		}

		~FScopedVanguardsTuning()
		{
			UVeyraVanguardsTuningSubsystem::SetTestOverride(nullptr);
		}

		FVeyraVanguardDefinition& Definition(const FVeyraContentId& Vanguard)
		{
			return Tuning.Vanguards.FindChecked(Vanguard);
		}

		UE_NONCOPYABLE(FScopedVanguardsTuning);
	};

	/** Abilities tuning a test sets up. Get() returns it while this object lives. */
	struct FScopedAbilitiesTuning
	{
		FVeyraAbilitiesTuning Tuning;

		FScopedAbilitiesTuning()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);
		}

		~FScopedAbilitiesTuning()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		UE_NONCOPYABLE(FScopedAbilitiesTuning);
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

	/**
	 * The join ticket a test gives the client in PIE instance PIEInstance. In-process play puts the
	 * dedicated server in instance 0 and the clients in 1 and up.
	 */
	inline FString TestTicketForPIEInstance(int32 PIEInstance)
	{
		return FString::Printf(TEXT("vjt_test_client_%d"), PIEInstance);
	}

	/** The account a test roster gives the client in PIE instance PIEInstance. */
	inline FString TestAccountForPIEInstance(int32 PIEInstance)
	{
		return FString::Printf(TEXT("test-account-%d"), PIEInstance);
	}

	/** Gives every PIE client its test join ticket while this object lives. */
	struct FScopedTestTickets
	{
		FScopedTestTickets()
		{
			UVeyraLocalPlayer::SetTestTicketProvider([](const ULocalPlayer& Player)
			{
				const UGameInstance* GameInstance = Player.GetGameInstance();
				const FWorldContext* Context = GameInstance ? GameInstance->GetWorldContext() : nullptr;
				return Context ? TestTicketForPIEInstance(Context->PIEInstance) : FString();
			});
		}

		~FScopedTestTickets()
		{
			UVeyraLocalPlayer::SetTestTicketProvider(nullptr);
		}

		UE_NONCOPYABLE(FScopedTestTickets);
	};

	/**
	 * Makes the test server host a match whose roster is the PIE clients, the Nth on Sides[N] as
	 * Vanguards[N] (the test Vanguard where none is given) taking FluxSpells[N] (none where none is
	 * given), and which adds Bots, while this object lives. A practice match's host is the first client. Set it before the network starts: the server
	 * reads it when the map loads.
	 */
	struct FScopedMatchAssignment
	{
		FVeyraMatchAssignment Assignment;
		TArray<FString> Problems;

		explicit FScopedMatchAssignment(TConstArrayView<EVeyraTeam> Sides, EVeyraMatchRules Rules = EVeyraMatchRules::Standard,
			TConstArrayView<FVeyraContentId> Vanguards = {}, TConstArrayView<FVeyraAssignedBot> Bots = {},
			TConstArrayView<TArray<FVeyraContentId>> FluxSpells = {}, TOptional<FVeyraCustomSettings> Custom = {})
		{
			Assignment.MatchId = TEXT("test-match");
			Assignment.Mode = FVeyraContentId::FromText(TEXT("test_mode")).GetValue();
			Assignment.Rules = Rules;
			Assignment.Bots = TArray<FVeyraAssignedBot>(Bots);
			for (int32 Index = 0; Index < Sides.Num(); ++Index)
			{
				const int32 PIEInstance = Index + 1;
				Assignment.Participants.Add({ TestAccountForPIEInstance(PIEInstance), FString::Printf(TEXT("TestPlayer%d"), PIEInstance),
					Sides[Index], VeyraHash::Sha256Hex(TestTicketForPIEInstance(PIEInstance)),
					Vanguards.IsValidIndex(Index) ? Vanguards[Index] : TestVanguardId() });
				if (FluxSpells.IsValidIndex(Index))
				{
					Assignment.Participants.Last().FluxSpells = FluxSpells[Index];
				}
			}
			if (VeyraMatchRules::HasHost(Rules) && !Assignment.Participants.IsEmpty())
			{
				Assignment.HostAccountId = Assignment.Participants[0].AccountId;
			}
			// A custom match carries its session's rules, open-ended unless a test says otherwise.
			if (Rules == EVeyraMatchRules::Custom)
			{
				Assignment.Custom = Custom.IsSet() ? Custom : TOptional<FVeyraCustomSettings>(FVeyraCustomSettings());
			}
			Problems = UVeyraMatchHostSubsystem::Get()->SetAssignment(Assignment);
		}

		~FScopedMatchAssignment()
		{
			UVeyraMatchHostSubsystem::Get()->ClearAssignment();
		}

		UE_NONCOPYABLE(FScopedMatchAssignment);
	};

	/** A developer match: a dedicated server running AVeyraGameMode and ClientCount clients. */
	template <typename StateType>
	void BuildMatchNetwork(FPIENetworkComponent<StateType>& Network, int32 ClientCount = MatchClientCount)
	{
		FNetworkComponentBuilder<StateType>()
			.WithClients(ClientCount)
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
