// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "AbilitySystemComponent.h"
#include "Algo/AllOf.h"
#include "Algo/Count.h"
#include "Attributes/VeyraVitalsSet.h"
#include "EngineUtils.h"
#include "Targeting/VeyraVisibility.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "Tuning/VeyraVisionTuningSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "VeyraGameState.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardCharacter.h"
#include "VeyraVisionSubsystem.h"

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
		static constexpr double NegativeCheckRealSeconds = 0.5;
		double HoldStartRealTime = 0.0;

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

		/** The Health this machine holds for the participant with PlayerId, or -1 if it has no participant. */
		static double SeenHealth(const UWorld* World, int32 PlayerId)
		{
			const AVeyraGameState* GameState = GameStateOf(World);
			for (const APlayerState* Candidate : GameState ? GameState->PlayerArray : TArray<TObjectPtr<APlayerState>>())
			{
				if (const AVeyraPlayerState* Participant = Cast<AVeyraPlayerState>(Candidate); Participant && Participant->GetPlayerId() == PlayerId)
				{
					return Participant->GetAbilitySystemComponent()->GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute());
				}
			}
			return -1.0;
		}

		/** Hits the enemy for Amount of True damage, from the observer, on the server. */
		void HitEnemy(const FState& State, double Amount)
		{
			FVeyraRawDamageEvent Hit;
			Hit.Components.Add({ EVeyraDamageType::TrueDamage, Amount });
			AVeyraPlayerState* Attacker = ServerControllerOf(State, ObserverIndex)->GetPlayerState<AVeyraPlayerState>();
			AVeyraPlayerState* Enemy = ServerControllerOf(State, EnemyIndex)->GetPlayerState<AVeyraPlayerState>();
			ASSERT_THAT(IsTrue(Attacker && Enemy && VeyraCombat::DealDamage(*Attacker->GetAbilitySystemComponent(), *Enemy->GetAbilitySystemComponent(), Hit)));
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

		TEST_METHOD(ADenseFogSightingReachesOnlyThePlayerInsideIt)
		{
			// The bush (Vision Bible §2; ADR-016 §3): the observer steps into the fog beside the enemy; its
			// teammate stays a few steps out, well within sight, and still never receives it.
			const FVector2D Bush(-SightRadius(), 0.0);
			const double BushRadius = 300.0;
			IdentifyPlayers(StartMatch(Network, Layout, EVeyraMatchPhase::Live))
				.ThenServer(TEXT("Grow a bush; the observer and the enemy go in, the bystander stays out"), [this, Bush, BushRadius](FState& State) {
					UVeyraVisionSubsystem* Vision = State.World->GetSubsystem<UVeyraVisionSubsystem>();
					ASSERT_THAT(IsTrue(Vision && Vision->IsStarted()));
					Vision->SetDenseFog({ FVeyraFogCircle{ Bush, BushRadius } });
					Place(State, ObserverIndex, Bush - FVector2D(100.0, 0.0));
					Place(State, EnemyIndex, Bush + FVector2D(100.0, 0.0));
					Place(State, BystanderIndex, Bush - FVector2D(BushRadius * 2.0, 0.0));
				})
				.UntilClients(TEXT("The observer receives the enemy in the fog"), [this](FState& State) {
					return State.ClientIndex != ObserverIndex || HasVanguard(State.World, Participants[EnemyIndex].PlayerId);
				})
				.ThenServer([this](FState& State) { HoldStartRealTime = State.World->GetRealTimeSeconds(); })
				.UntilServer(TEXT("Give the sighting time to leak"), [this](FState& State) { return State.World->GetRealTimeSeconds() - HoldStartRealTime >= NegativeCheckRealSeconds; })
				.ThenClients(TEXT("Its teammate outside the fog never has it"), [this](FState& State) {
					if (State.ClientIndex == BystanderIndex)
					{
						ASSERT_THAT(IsFalse(HasVanguard(State.World, Participants[EnemyIndex].PlayerId)));
					}
				});
		}

		TEST_METHOD(AnEnemysHealthReachesOnlyThoseWhoSeeIt)
		{
			// A participant's data behind the fog (ADR-006 §5, ADR-016 §3): its PlayerState reaches everyone,
			// its Health only those who see its Vanguard; out of sight, a viewer keeps what it last saw.
			const FVector2D Observer(-SightRadius(), 0.0);
			const FVector2D Bystander(-SightRadius(), SightRadius() / 3.0);
			const FVector2D Far(SightRadius() * 1.5, 0.0);
			const FVector2D Near(-SightRadius() / 2.0, 0.0);
			const double MaxHealth = TestVanguard().BaseStats.MaxHealth;
			constexpr double HitAmount = 50.0;
			const auto EnemyUnseen = [this](FState& State) {
				const AVeyraVanguardCharacter* Enemy = FindVanguard(State.World, Participants[EnemyIndex].PlayerId);
				return Enemy && !VeyraVisibility::IsVisibleToTeam(Participants[ObserverIndex].Team, *Enemy);
			};
			const auto Hold = [this](FPIENetworkComponent<FState>& Chain) -> FPIENetworkComponent<FState>& {
				return Chain.ThenServer([this](FState& State) { HoldStartRealTime = State.World->GetRealTimeSeconds(); })
					.UntilServer(TEXT("Give it time to leak"), [this](FState& State) { return State.World->GetRealTimeSeconds() - HoldStartRealTime >= NegativeCheckRealSeconds; });
			};
			FPIENetworkComponent<FState>& Hidden = IdentifyPlayers(StartMatch(Network, Layout, EVeyraMatchPhase::Live))
				.ThenServer(TEXT("Part the sides"), [this, Observer, Bystander, Far](FState& State) {
					Place(State, ObserverIndex, Observer);
					Place(State, BystanderIndex, Bystander);
					Place(State, EnemyIndex, Far);
				})
				.UntilServer(TEXT("Vision loses the enemy"), EnemyUnseen)
				.ThenServer(TEXT("Hit it out of sight"), [this, HitAmount](FState& State) { HitEnemy(State, HitAmount); })
				.UntilClients(TEXT("Its own client sees its Health fall"), [this, MaxHealth, HitAmount](FState& State) {
					return State.ClientIndex != EnemyIndex || SeenHealth(State.World, Participants[EnemyIndex].PlayerId) == MaxHealth - HitAmount;
				});
			FPIENetworkComponent<FState>& Seen = Hold(Hidden)
				.ThenClients(TEXT("Nobody who cannot see it learned its Health"), [this, MaxHealth, HitAmount](FState& State) {
					if (State.ClientIndex != EnemyIndex)
					{
						ASSERT_THAT(IsTrue(SeenHealth(State.World, Participants[EnemyIndex].PlayerId) != MaxHealth - HitAmount));
					}
				})
				.ThenServer(TEXT("It walks into sight"), [this, Near](FState& State) { Place(State, EnemyIndex, Near); })
				.UntilClients(TEXT("Every client now has its Health"), [this, MaxHealth, HitAmount](FState& State) {
					return SeenHealth(State.World, Participants[EnemyIndex].PlayerId) == MaxHealth - HitAmount;
				})
				.ThenServer(TEXT("It walks away"), [this, Far](FState& State) { Place(State, EnemyIndex, Far); })
				.UntilServer(TEXT("Vision loses it again"), EnemyUnseen)
				.ThenServer(TEXT("Hit it again"), [this, HitAmount](FState& State) { HitEnemy(State, HitAmount); })
				.UntilClients(TEXT("Its own client sees the second hit"), [this, MaxHealth, HitAmount](FState& State) {
					return State.ClientIndex != EnemyIndex || SeenHealth(State.World, Participants[EnemyIndex].PlayerId) == MaxHealth - 2.0 * HitAmount;
				});
			Hold(Seen).ThenClients(TEXT("The others keep the Health they last saw"), [this, MaxHealth, HitAmount](FState& State) {
				if (State.ClientIndex != EnemyIndex)
				{
					ASSERT_THAT(IsTrue(SeenHealth(State.World, Participants[EnemyIndex].PlayerId) == MaxHealth - HitAmount));
				}
			});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
