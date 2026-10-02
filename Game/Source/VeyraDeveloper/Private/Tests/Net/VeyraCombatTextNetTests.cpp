// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "Absorption/VeyraAbsorptionLedger.h"
#include "AbilitySystemComponent.h"
#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"
#include "Algo/Count.h"
#include "Feedback/VeyraCombatTextTypes.h"
#include "Targeting/VeyraVisibility.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "Tuning/VeyraVisionTuningSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "VeyraPlayerController.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardCharacter.h"

namespace VeyraNetTests
{
	// Veyra.Net.CombatText.*: floating combat text (ADR-052 §1). Each number reaches only the player it concerns, and
	// none about a unit that player's side cannot see. Three players, so one side has two: the observer and the
	// bystander together, the enemy on its own.
	NETWORK_TEST_CLASS(CombatText, "Veyra.Net")
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
		static constexpr double HitAmount = 50.0;
		static constexpr double HealAmount = 20.0;
		static constexpr double ShieldAmount = 30.0;
		static constexpr double ShieldSeconds = 60.0;
		double HoldStartRealTime = 0.0;

		struct FParticipant
		{
			int32 PlayerId = INDEX_NONE;
			EVeyraTeam Team = EVeyraTeam::None;
		};
		TArray<FParticipant> Participants;
		int32 ObserverIndex = INDEX_NONE;
		int32 BystanderIndex = INDEX_NONE;
		int32 EnemyIndex = INDEX_NONE;

		/** The numbers each client's player received, by client. */
		TArray<TArray<FVeyraCombatTextLine>> Received;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			// The fog matters here: the committed sight, not the see-everything fixture.
			Tuning->Vision.Tuning = Tuning->Vision.Committed;
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(PlayerCount);
			Received.SetNum(PlayerCount);
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

		void Place(const FState& State, int32 ClientIndex, const FVector2D& Where)
		{
			AVeyraVanguardCharacter* Pawn = FindVanguard(State.World, Participants[ClientIndex].PlayerId);
			ASSERT_THAT(IsNotNull(Pawn));
			Pawn->SetActorLocation(FVector(Where.X, Where.Y, Pawn->GetActorLocation().Z), /*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);
		}

		static UAbilitySystemComponent& UnitOf(const FState& State, int32 ClientIndex)
		{
			return *ServerControllerOf(State, ClientIndex)->GetPlayerState<AVeyraPlayerState>()->GetAbilitySystemComponent();
		}

		/** Hits the enemy for HitAmount of True damage from the observer, on the server. */
		void HitEnemy(const FState& State)
		{
			FVeyraRawDamageEvent Hit;
			Hit.Components.Add({ EVeyraDamageType::TrueDamage, HitAmount });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(UnitOf(State, ObserverIndex), UnitOf(State, EnemyIndex), Hit)));
		}

		/** The numbers of Kind the client at ClientIndex received. */
		int32 CountOf(int32 ClientIndex, EVeyraCombatTextKind Kind) const
		{
			return Algo::CountIf(Received[ClientIndex], [Kind](const FVeyraCombatTextLine& Line) { return Line.Kind == Kind; });
		}

		/** Records each client's participant, picks the observer, bystander and enemy, and listens on each client. */
		FPIENetworkComponent<FState>& Prepare(FPIENetworkComponent<FState>& Chain)
		{
			return Chain
				.ThenServer(TEXT("Identify the players"), [this](FState& State) {
					Participants.Reset();
					for (int32 Index = 0; Index < PlayerCount; ++Index)
					{
						const AVeyraPlayerState& Participant = *ServerControllerOf(State, Index)->GetPlayerState<AVeyraPlayerState>();
						Participants.Add({ Participant.GetPlayerId(), Participant.GetVeyraTeam() });
					}
					for (int32 Index = 0; Index < PlayerCount; ++Index)
					{
						const int32 SameSide = Algo::CountIf(Participants, [this, Index](const FParticipant& Other) { return Other.Team == Participants[Index].Team; });
						int32& Role = SameSide == 1 ? EnemyIndex : ObserverIndex == INDEX_NONE ? ObserverIndex : BystanderIndex;
						Role = Index;
					}
					ASSERT_THAT(IsTrue(EnemyIndex != INDEX_NONE && ObserverIndex != INDEX_NONE && BystanderIndex != INDEX_NONE));
				})
				.ThenClients(TEXT("Listen for combat text"), [this](FState& State) {
					LocalControllerOf(State.World)->OnCombatText.AddLambda([this, Index = State.ClientIndex](const FVeyraCombatTextLine& Line) { Received[Index].Add(Line); });
				});
		}

		/** Waits long enough for a number that should not come to have come. */
		FPIENetworkComponent<FState>& Hold(FPIENetworkComponent<FState>& Chain)
		{
			return Chain.ThenServer([this](FState& State) { HoldStartRealTime = State.World->GetRealTimeSeconds(); })
				.UntilServer(TEXT("Give it time to arrive"), [this](FState& State) { return State.World->GetRealTimeSeconds() - HoldStartRealTime >= NegativeCheckRealSeconds; });
		}

		TEST_METHOD(AHitReachesItsDealerAndItsReceiverAndNoOneElse)
		{
			const FVector2D Observer(-SightRadius() / 2.0, 0.0);
			const FVector2D Bystander(-SightRadius() / 2.0, SightRadius() / 4.0);
			const FVector2D Enemy(0.0, 0.0);
			FPIENetworkComponent<FState>& Hit = Prepare(StartMatch(Network, Layout, EVeyraMatchPhase::Live))
				.ThenServer(TEXT("Bring everyone together"), [this, Observer, Bystander, Enemy](FState& State) {
					Place(State, ObserverIndex, Observer);
					Place(State, BystanderIndex, Bystander);
					Place(State, EnemyIndex, Enemy);
				})
				.UntilServer(TEXT("Each side sees the other"), [this](FState& State) {
					const AVeyraVanguardCharacter* Target = FindVanguard(State.World, Participants[EnemyIndex].PlayerId);
					const AVeyraVanguardCharacter* Dealer = FindVanguard(State.World, Participants[ObserverIndex].PlayerId);
					return Target && Dealer && VeyraVisibility::IsVisibleToTeam(Participants[ObserverIndex].Team, *Target)
						&& VeyraVisibility::IsVisibleToTeam(Participants[EnemyIndex].Team, *Dealer);
				})
				// A number names its units, so every client must hold them first.
				.UntilClients(TEXT("Every client has every Vanguard"), [this](FState& State) {
					return Algo::AllOf(Participants, [&State](const FParticipant& Each) { return FindVanguard(State.World, Each.PlayerId) != nullptr; });
				})
				.ThenServer(TEXT("The observer hits the enemy"), [this](FState& State) { HitEnemy(State); })
				.UntilClients(TEXT("The dealer and the receiver each see the hit"), [this](FState& State) {
					const int32 Index = State.ClientIndex;
					const EVeyraCombatTextKind Kind = Index == ObserverIndex ? EVeyraCombatTextKind::DamageDealt : EVeyraCombatTextKind::DamageReceived;
					return Index == BystanderIndex || Algo::AnyOf(Received[Index], [this, Kind, &State](const FVeyraCombatTextLine& Line) {
						return Line.Kind == Kind && FMath::IsNearlyEqual(Line.Amount, static_cast<float>(HitAmount)) && Line.DamageType == EVeyraDamageType::TrueDamage
							&& Line.Unit.Get() == FindVanguard(State.World, Participants[EnemyIndex].PlayerId) && Line.Other.Get() == FindVanguard(State.World, Participants[ObserverIndex].PlayerId);
					});
				});
			Hold(Hit).ThenClients(TEXT("No one else saw it"), [this](FState& State) {
				ASSERT_THAT(IsTrue(Received[BystanderIndex].IsEmpty(), TEXT("an ally who took no part")));
				ASSERT_THAT(IsTrue(CountOf(ObserverIndex, EVeyraCombatTextKind::DamageDealt) == 1 && CountOf(ObserverIndex, EVeyraCombatTextKind::DamageReceived) == 0));
				ASSERT_THAT(IsTrue(CountOf(EnemyIndex, EVeyraCombatTextKind::DamageReceived) == 1 && CountOf(EnemyIndex, EVeyraCombatTextKind::DamageDealt) == 0));
			});
		}

		TEST_METHOD(SelfDamageReachesItsPlayerAsReceived)
		{
			FPIENetworkComponent<FState>& Hurt = Prepare(StartMatch(Network, Layout, EVeyraMatchPhase::Live))
				.ThenServer(TEXT("The observer hurts itself"), [this](FState& State) {
					FVeyraRawDamageEvent Cost;
					Cost.Components.Add({ EVeyraDamageType::TrueDamage, HitAmount });
					ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(UnitOf(State, ObserverIndex), UnitOf(State, ObserverIndex), Cost)));
				})
				.UntilClients(TEXT("It sees what it took"), [this](FState& State) {
					return State.ClientIndex != ObserverIndex || CountOf(ObserverIndex, EVeyraCombatTextKind::DamageReceived) > 0;
				});
			Hold(Hurt).ThenClients(TEXT("Only as received, and no one else saw it"), [this](FState& State) {
				ASSERT_THAT(IsTrue(CountOf(ObserverIndex, EVeyraCombatTextKind::DamageReceived) == 1 && CountOf(ObserverIndex, EVeyraCombatTextKind::DamageDealt) == 0));
				ASSERT_THAT(IsTrue(Received[BystanderIndex].IsEmpty() && Received[EnemyIndex].IsEmpty()));
			});
		}

		TEST_METHOD(AHitOnAUnitInTheFogSendsItsDealerNothing)
		{
			const FVector2D Observer(-SightRadius(), 0.0);
			const FVector2D Bystander(-SightRadius(), SightRadius() / 3.0);
			const FVector2D Far(SightRadius() * 1.5, 0.0);
			FPIENetworkComponent<FState>& Hit = Prepare(StartMatch(Network, Layout, EVeyraMatchPhase::Live))
				.ThenServer(TEXT("Part the sides"), [this, Observer, Bystander, Far](FState& State) {
					Place(State, ObserverIndex, Observer);
					Place(State, BystanderIndex, Bystander);
					Place(State, EnemyIndex, Far);
				})
				.UntilServer(TEXT("Neither side sees the other"), [this](FState& State) {
					const AVeyraVanguardCharacter* Target = FindVanguard(State.World, Participants[EnemyIndex].PlayerId);
					const AVeyraVanguardCharacter* Dealer = FindVanguard(State.World, Participants[ObserverIndex].PlayerId);
					return Target && Dealer && !VeyraVisibility::IsVisibleToTeam(Participants[ObserverIndex].Team, *Target)
						&& !VeyraVisibility::IsVisibleToTeam(Participants[EnemyIndex].Team, *Dealer);
				})
				.ThenServer(TEXT("The observer hits the enemy out of sight"), [this](FState& State) { HitEnemy(State); })
				.UntilClients(TEXT("The enemy sees what it took, but not from whom"), [this](FState& State) {
					return State.ClientIndex != EnemyIndex || Algo::AnyOf(Received[EnemyIndex], [](const FVeyraCombatTextLine& Line) {
						return Line.Kind == EVeyraCombatTextKind::DamageReceived && Line.Other == nullptr;
					});
				});
			Hold(Hit).ThenClients(TEXT("Its dealer learned nothing of a unit in the fog"), [this](FState& State) {
				ASSERT_THAT(IsTrue(Received[ObserverIndex].IsEmpty() && Received[BystanderIndex].IsEmpty()));
			});
		}

		TEST_METHOD(HealingAndShieldsReachBothEnds)
		{
			const FVector2D Observer(-SightRadius(), 0.0);
			const FVector2D Bystander(-SightRadius(), SightRadius() / 3.0);
			const FVector2D Far(SightRadius() * 1.5, 0.0);
			FPIENetworkComponent<FState>& Given = Prepare(StartMatch(Network, Layout, EVeyraMatchPhase::Live))
				.ThenServer(TEXT("Part the sides"), [this, Observer, Bystander, Far](FState& State) {
					Place(State, ObserverIndex, Observer);
					Place(State, BystanderIndex, Bystander);
					Place(State, EnemyIndex, Far);
				})
				.ThenServer(TEXT("The bystander is hurt, then healed and shielded by the observer"), [this](FState& State) {
					UAbilitySystemComponent& Healer = UnitOf(State, ObserverIndex);
					UAbilitySystemComponent& Ally = UnitOf(State, BystanderIndex);
					FVeyraRawDamageEvent Wound;
					Wound.Components.Add({ EVeyraDamageType::TrueDamage, HitAmount });
					ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(UnitOf(State, EnemyIndex), Ally, Wound)));
					ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraCombat::RestoreHealthFrom(Healer, Ally, HealAmount), HealAmount)));
					ASSERT_THAT(IsTrue(VeyraCombat::GrantShield(Healer, Ally, EVeyraShieldCategory::Universal, ShieldAmount, ShieldSeconds).IsValid()));
					// Regeneration heals no one in particular, and shows nothing.
					ASSERT_THAT(IsTrue(VeyraCombat::RestoreHealth(Ally, HealAmount)));
				})
				.UntilClients(TEXT("The healer and the healed each see both"), [this](FState& State) {
					const int32 Index = State.ClientIndex;
					return Index == EnemyIndex || (CountOf(Index, EVeyraCombatTextKind::Healing) >= 1 && CountOf(Index, EVeyraCombatTextKind::Shielding) >= 1);
				});
			Hold(Given).ThenClients(TEXT("Once each, and nothing reached the other side"), [this](FState& State) {
				for (const int32 Index : { ObserverIndex, BystanderIndex })
				{
					ASSERT_THAT(IsTrue(CountOf(Index, EVeyraCombatTextKind::Healing) == 1 && CountOf(Index, EVeyraCombatTextKind::Shielding) == 1));
					const FVeyraCombatTextLine* Heal = Received[Index].FindByPredicate([](const FVeyraCombatTextLine& Line) { return Line.Kind == EVeyraCombatTextKind::Healing; });
					ASSERT_THAT(IsTrue(Heal && FMath::IsNearlyEqual(Heal->Amount, static_cast<float>(HealAmount))));
				}
				ASSERT_THAT(IsTrue(CountOf(EnemyIndex, EVeyraCombatTextKind::Healing) == 0 && CountOf(EnemyIndex, EVeyraCombatTextKind::Shielding) == 0));
			});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
