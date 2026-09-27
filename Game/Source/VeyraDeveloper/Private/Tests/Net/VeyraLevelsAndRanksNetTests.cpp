// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "Engine/Engine.h"
#include "GameFramework/PlayerState.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Progression/VeyraProgressionTuningSubsystem.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "VeyraPlayerState.h"

namespace VeyraNetTests
{
	// Veyra.Net.LevelsAndRanks.*: the player's rank-up order and the developer XP commands
	// (Economy & Progression Bible §1, §9; ADR-008 §6). Named so because test class names must be
	// unique, and Veyra.Economy.Progression exists.
	NETWORK_TEST_CLASS(LevelsAndRanks, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		FVeyraGreyboxLayout Layout;

		// Fixture values: a short preparation, and the levels to the first ultimate rank.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr int32 LevelsToUltimate = 5;

		int32 PlayerId = INDEX_NONE;

		BEFORE_EACH()
		{
			IgnoreLoginViewTargetRpc(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			// Cairn, with the first rank left to the player.
			Tuning->Tuning.DeveloperMatch.Vanguards = { FVeyraContentId::FromText(TEXT("cairn")).GetValue() };
			Tuning->Tuning.DeveloperMatch.StartingRank = EVeyraDeveloperStartingRank::None;
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		static UVeyraProgressionComponent* ServerProgression(FState& State)
		{
			const AVeyraPlayerController* Controller = ServerControllerOf(State, 0);
			return Controller && Controller->PlayerState ? Controller->PlayerState->FindComponentByClass<UVeyraProgressionComponent>() : nullptr;
		}

		/** The first client's progression, as this machine sees it. */
		const UVeyraProgressionComponent* SeenProgression(FState& State) const
		{
			for (const APlayerState* Participant : GameStateOf(State.World)->PlayerArray)
			{
				if (Participant && Participant->GetPlayerId() == PlayerId)
				{
					return Participant->FindComponentByClass<UVeyraProgressionComponent>();
				}
			}
			return nullptr;
		}

		FPIENetworkComponent<FState>& Identify(FPIENetworkComponent<FState>& Chain)
		{
			return Chain.ThenServer(TEXT("Identify the player"), [this](FState& State) { PlayerId = ServerControllerOf(State, 0)->PlayerState->GetPlayerId(); });
		}

		TEST_METHOD(TheUltimateWaitsForItsLevel)
		{
			Identify(StartMatch(Network, Layout, EVeyraMatchPhase::Live))
				.ThenClient(TEXT("Try the ultimate at level 1"), 0, [](FState& State) { LocalControllerOf(State.World)->RequestRankUp(EVeyraAbilitySlot::R); })
				.UntilClient(TEXT("The server refuses it"), 0, [](FState& State) {
					return LocalControllerOf(State.World)->GetLastRankUpRefusal() == EVeyraRankRefusal::LevelTooLow;
				})
				.ThenClient(TEXT("Take levels"), 0, [](FState& State) { LocalControllerOf(State.World)->RequestDeveloperLevels(LevelsToUltimate); })
				.UntilServer(TEXT("The server grants them"), [](FState& State) { return ServerProgression(State)->GetLevel() == 1 + LevelsToUltimate; })
				.ThenClient(TEXT("Try the ultimate again"), 0, [](FState& State) { LocalControllerOf(State.World)->RequestRankUp(EVeyraAbilitySlot::R); })
				.UntilClients(TEXT("Every client sees its first rank"), [this](FState& State) {
					const UVeyraProgressionComponent* Progression = SeenProgression(State);
					return Progression && Progression->GetRank(EVeyraAbilitySlot::R) == 1 && Progression->GetLevel() == 1 + LevelsToUltimate;
				});
		}

		TEST_METHOD(ASkillPointIsSpentOnce)
		{
			Identify(StartMatch(Network, Layout, EVeyraMatchPhase::Live))
				.ThenClient(TEXT("Rank up Q with the level's point"), 0, [](FState& State) { LocalControllerOf(State.World)->RequestRankUp(EVeyraAbilitySlot::Q); })
				.UntilServer(TEXT("Q is ranked"), [](FState& State) { return ServerProgression(State)->GetRank(EVeyraAbilitySlot::Q) == 1; })
				.ThenClient(TEXT("Try W with no point left"), 0, [](FState& State) { LocalControllerOf(State.World)->RequestRankUp(EVeyraAbilitySlot::W); })
				.UntilClient(TEXT("The server refuses it"), 0, [](FState& State) {
					return LocalControllerOf(State.World)->GetLastRankUpRefusal() == EVeyraRankRefusal::NoSkillPoint;
				})
				.ThenServer(TEXT("W stays unranked"), [this](FState& State) { ASSERT_THAT(AreEqual(0, ServerProgression(State)->GetRank(EVeyraAbilitySlot::W))); });
		}

		TEST_METHOD(TheDeveloperCommandsGrantExperience)
		{
			const int32 Partial = UVeyraProgressionTuningSubsystem::Get().Experience.ToNextLevel[0] / 2;
			Identify(StartMatch(Network, Layout, EVeyraMatchPhase::Live))
				.ThenClient(TEXT("Grant XP from the console"), 0, [Partial](FState& State) {
					GEngine->Exec(State.World, *FString::Printf(TEXT("Veyra.Dev.GrantXp %d"), Partial));
				})
				.UntilServer(TEXT("The server grants it"), [Partial](FState& State) { return ServerProgression(State)->GetExperience() == Partial; })
				.ThenClient(TEXT("Grant levels from the console"), 0, [](FState& State) { GEngine->Exec(State.World, TEXT("Veyra.Dev.GrantLevels 2")); })
				.UntilServer(TEXT("Two levels later"), [](FState& State) {
					const UVeyraProgressionComponent* Progression = ServerProgression(State);
					return Progression->GetLevel() == 3 && Progression->GetExperience() == 0;
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
