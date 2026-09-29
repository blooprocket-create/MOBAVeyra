// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "AbilitySystemComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Rewards/VeyraEconomyTuningSubsystem.h"
#include "Statistics/VeyraMatchStatisticsSubsystem.h"
#include "Statistics/VeyraScoreComponent.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "VeyraCombatVerbs.h"
#include "VeyraPlayerState.h"

namespace VeyraNetTests
{
	// Veyra.Net.Statistics.*: the match records every participant from its preparation, and each one's
	// K/D/A and last hits reach every client, seen or not, as League's scoreboard shows them (ADR-017 §3).
	NETWORK_TEST_CLASS(Statistics, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		FVeyraGreyboxLayout Layout;

		// Fixture values.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr double Lethal = 100000.0;

		int32 VictimId = INDEX_NONE;
		int32 KillerId = INDEX_NONE;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		static AVeyraPlayerState& ServerParticipant(FState& State, int32 ClientIndex)
		{
			return *ServerControllerOf(State, ClientIndex)->GetPlayerState<AVeyraPlayerState>();
		}

		static const FVeyraScore* ScoreOf(const UWorld* World, int32 PlayerId)
		{
			const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
			for (const APlayerState* Member : GameState ? GameState->PlayerArray : TArray<TObjectPtr<APlayerState>>())
			{
				const UVeyraScoreComponent* Score = Member && Member->GetPlayerId() == PlayerId ? Member->FindComponentByClass<UVeyraScoreComponent>() : nullptr;
				if (Score)
				{
					return &Score->GetScore();
				}
			}
			return nullptr;
		}

		TEST_METHOD(AKillReachesTheRecordAndEveryScoreboard)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Each participant has a record from its starting Gold"), [this](FState& State) {
					const UVeyraMatchStatisticsSubsystem* Service = State.World->GetSubsystem<UVeyraMatchStatisticsSubsystem>();
					ASSERT_THAT(IsTrue(Service && Service->IsRecording()));
					for (int32 Client = 0; Client < MatchClientCount; ++Client)
					{
						const TOptional<FVeyraPlayerStatistics> Record = Service->Snapshot(ServerParticipant(State, Client));
						ASSERT_THAT(IsTrue(Record.IsSet()));
						ASSERT_THAT(IsTrue(Record->GoldBySource.Starting == UVeyraEconomyTuningSubsystem::Get().Gold.Starting, TEXT("starting Gold counts as earned")));
						ASSERT_THAT(IsTrue(Record->Level >= 1 && Record->FluxSpells.Num() == 2));
					}
				})
				.ThenServer(TEXT("One participant kills the other"), [this](FState& State) {
					AVeyraPlayerState& Victim = ServerParticipant(State, 0);
					AVeyraPlayerState& Killer = ServerParticipant(State, 1);
					VictimId = Victim.GetPlayerId();
					KillerId = Killer.GetPlayerId();
					FVeyraRawDamageEvent Damage;
					Damage.Components.Add({ EVeyraDamageType::TrueDamage, Lethal });
					ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Killer.GetAbilitySystemComponent(), *Victim.GetAbilitySystemComponent(), Damage)));
					const UVeyraMatchStatisticsSubsystem* Service = State.World->GetSubsystem<UVeyraMatchStatisticsSubsystem>();
					ASSERT_THAT(IsTrue(Service->Snapshot(Killer)->Kills == 1 && Service->Snapshot(Victim)->Deaths == 1));
				})
				.UntilClients(TEXT("Every client's scoreboard shows the kill and the death"), [this](FState& State) {
					const FVeyraScore* Killer = ScoreOf(State.World, KillerId);
					const FVeyraScore* Victim = ScoreOf(State.World, VictimId);
					return Killer && Victim && Killer->Kills == 1 && Victim->Deaths == 1;
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
