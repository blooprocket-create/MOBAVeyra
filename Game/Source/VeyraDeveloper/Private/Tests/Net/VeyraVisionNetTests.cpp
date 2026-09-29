// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "Algo/AllOf.h"
#include "Algo/Count.h"
#include "EngineUtils.h"
#include "Targeting/VeyraVisibility.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "Tuning/VeyraVisionTuningSubsystem.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardCharacter.h"

namespace VeyraNetTests
{
	// Veyra.Net.Vision.*: the fog gate (ADR-016 §3). An enemy Vanguard reaches a player's client only
	// while that player's side sees it; its own side always has it. Three players, so one side has two:
	// team vision is shared, so what one of them sees reaches the other too.
	NETWORK_TEST_CLASS(Vision, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		FVeyraGreyboxLayout Layout;

		static constexpr int32 PlayerCount = 3;
		static constexpr double ShortPreparationSeconds = 0.1;

		struct FParticipant
		{
			int32 PlayerId = INDEX_NONE;
			EVeyraTeam Team = EVeyraTeam::None;
		};
		TArray<FParticipant> Participants;
		// The two members of the larger side, and the player on the other side.
		int32 ObserverIndex = INDEX_NONE;
		int32 BystanderIndex = INDEX_NONE;
		int32 EnemyIndex = INDEX_NONE;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			// This test is about fog: the committed sight, not the see-everything fixture.
			Tuning->Vision.Tuning = Tuning->Vision.Committed;
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(PlayerCount);
			BuildMatchNetwork(Network, PlayerCount);
		}

		AFTER_EACH()
		{
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		static double SightRadius()
		{
			return UVeyraVisionTuningSubsystem::Get().Sight.Vanguard;
		}

		/** Moves ClientIndex's Vanguard, on the server, to Where on the ground plane. */
		void Place(const FState& State, int32 ClientIndex, const FVector2D& Where)
		{
			// A server-side controller possesses it, not the player's (ADR-006 §7).
			AVeyraVanguardCharacter* Pawn = FindVanguard(State.World, Participants[ClientIndex].PlayerId);
			ASSERT_THAT(IsNotNull(Pawn));
			const FVector Location(Where.X, Where.Y, Pawn->GetActorLocation().Z);
			Pawn->SetActorLocation(Location, /*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);
		}

		/** Whether this machine has the Vanguard of the participant with PlayerId. */
		static bool HasVanguard(const UWorld* World, int32 PlayerId)
		{
			for (TActorIterator<AVeyraVanguardCharacter> It(World); It; ++It)
			{
				const APlayerState* Owner = It->GetPlayerState();
				if (Owner && Owner->GetPlayerId() == PlayerId)
				{
					return true;
				}
			}
			return false;
		}

		/** Records each client's participant and picks the observer, bystander and enemy. */
		FPIENetworkComponent<FState>& IdentifyPlayers(FPIENetworkComponent<FState>& Chain)
		{
			return Chain.ThenServer(TEXT("Identify the players"), [this](FState& State) {
				Participants.Reset();
				for (int32 Index = 0; Index < PlayerCount; ++Index)
				{
					const AVeyraPlayerState& Participant = *ServerControllerOf(State, Index)->GetPlayerState<AVeyraPlayerState>();
					Participants.Add({ Participant.GetPlayerId(), Participant.GetVeyraTeam() });
				}
				for (int32 Index = 0; Index < PlayerCount; ++Index)
				{
					const int32 SameSide = Algo::CountIf(Participants, [this, Index](const FParticipant& Other) { return Other.Team == Participants[Index].Team; });
					if (SameSide == 1)
					{
						EnemyIndex = Index;
					}
					else if (ObserverIndex == INDEX_NONE)
					{
						ObserverIndex = Index;
					}
					else
					{
						BystanderIndex = Index;
					}
				}
				ASSERT_THAT(IsTrue(EnemyIndex != INDEX_NONE && ObserverIndex != INDEX_NONE && BystanderIndex != INDEX_NONE));
			});
		}

		TEST_METHOD(AnEnemyReachesAClientOnlyWhileItsSideSeesIt)
		{
			// Within the grey box's floor: the larger side together, the enemy far beyond its sight.
			const FVector2D Observer(-SightRadius(), 0.0);
			const FVector2D Bystander(-SightRadius(), SightRadius() / 3.0);
			const FVector2D Far(SightRadius() * 1.5, 0.0);
			const FVector2D Near(-SightRadius() / 2.0, 0.0);
			IdentifyPlayers(StartMatch(Network, Layout, EVeyraMatchPhase::Live))
				.ThenServer(TEXT("Part the sides"), [this, Observer, Bystander, Far](FState& State) {
					Place(State, ObserverIndex, Observer);
					Place(State, BystanderIndex, Bystander);
					Place(State, EnemyIndex, Far);
				})
				.UntilClients(TEXT("Each client has its own side's Vanguards and no enemy's"), [this](FState& State) {
					const EVeyraTeam MySide = Participants[State.ClientIndex].Team;
					return Algo::AllOf(Participants, [&State, MySide](const FParticipant& Other) {
						return HasVanguard(State.World, Other.PlayerId) == (Other.Team == MySide);
					});
				})
				.ThenServer(TEXT("The enemy walks into the observer's sight"), [this, Near](FState& State) { Place(State, EnemyIndex, Near); })
				.UntilClients(TEXT("Its whole side receives it, and it receives them"), [this](FState& State) {
					return Algo::AllOf(Participants, [&State](const FParticipant& Other) { return HasVanguard(State.World, Other.PlayerId); });
				})
				.ThenServer(TEXT("The enemy walks away"), [this, Far](FState& State) { Place(State, EnemyIndex, Far); })
				.UntilClients(TEXT("It leaves the other side's clients"), [this](FState& State) {
					const EVeyraTeam MySide = Participants[State.ClientIndex].Team;
					return Algo::AllOf(Participants, [&State, MySide](const FParticipant& Other) {
						return HasVanguard(State.World, Other.PlayerId) == (Other.Team == MySide);
					});
				});
		}

		TEST_METHOD(AnEnemyNobodySeesCannotBeOrderedAttacked)
		{
			const FVector2D Observer(-SightRadius(), 0.0);
			const FVector2D Bystander(-SightRadius(), SightRadius() / 3.0);
			const FVector2D Far(SightRadius() * 1.5, 0.0);
			const FVector2D Near(-SightRadius() / 2.0, 0.0);
			IdentifyPlayers(StartMatch(Network, Layout, EVeyraMatchPhase::Live))
				.ThenServer(TEXT("Part the sides"), [this, Observer, Bystander, Far](FState& State) {
					Place(State, ObserverIndex, Observer);
					Place(State, BystanderIndex, Bystander);
					Place(State, EnemyIndex, Far);
				})
				.UntilServer(TEXT("Vision loses the enemy"), [this](FState& State) {
					const AVeyraVanguardCharacter* Enemy = FindVanguard(State.World, Participants[EnemyIndex].PlayerId);
					return Enemy && !VeyraVisibility::IsVisibleToTeam(Participants[ObserverIndex].Team, *Enemy);
				})
				.ThenServer(TEXT("An attack order on it is refused, and it walks into sight"), [this, Near](FState& State) {
					AVeyraPlayerState* Attacker = ServerControllerOf(State, ObserverIndex)->GetPlayerState<AVeyraPlayerState>();
					AVeyraVanguardCharacter* Enemy = FindVanguard(State.World, Participants[EnemyIndex].PlayerId);
					ASSERT_THAT(IsTrue(Attacker && Enemy));
					// Nobody may target what they cannot see (Vision Bible §1).
					ASSERT_THAT(IsTrue(GameModeOf(State.World)->HandleAttackOrder(Attacker, Enemy) == EVeyraOrderRejection::CannotAttack));
					Place(State, EnemyIndex, Near);
				})
				.UntilServer(TEXT("Vision sees it"), [this](FState& State) {
					const AVeyraVanguardCharacter* Enemy = FindVanguard(State.World, Participants[EnemyIndex].PlayerId);
					return Enemy && VeyraVisibility::IsVisibleToTeam(Participants[ObserverIndex].Team, *Enemy);
				})
				.ThenServer(TEXT("Now the order stands"), [this](FState& State) {
					AVeyraPlayerState* Attacker = ServerControllerOf(State, ObserverIndex)->GetPlayerState<AVeyraPlayerState>();
					ASSERT_THAT(IsTrue(GameModeOf(State.World)->HandleAttackOrder(Attacker, FindVanguard(State.World, Participants[EnemyIndex].PlayerId))
						== EVeyraOrderRejection::None));
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
