// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "AbilitySystemComponent.h"
#include "Algo/AllOf.h"
#include "Algo/Count.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Engine/NetConnection.h"
#include "EngineUtils.h"
#include "Iris/ReplicationSystem/Filtering/NetObjectFilter.h"
#include "Iris/ReplicationSystem/ObjectReplicationBridge.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Net/Core/Misc/NetConditionGroupManager.h"
#include "Net/Iris/ReplicationSystem/ReplicationSystemUtil.h"
#include "Net/Subsystems/NetworkSubsystem.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "VeyraCombatVerbs.h"
#include "VeyraPlayerState.h"

namespace VeyraNetTests
{
	// Veyra.Net.FogGate.*: the ADR-006 §5 spike. How Iris expresses the per-player fog gate: an enemy
	// reaches a player's client only while that player sees it. Vision does not exist yet, so the
	// test drives Iris the way Vision would. Three players, so one side has two: a sighting by one
	// of them must not reach the other.
	//
	// Two mechanisms are under test:
	// - Units (root objects): the engine's filter-out dynamic filter hides them from everyone by
	//   default. Inclusion groups override it: one per side, allowed for that side's connections, and
	//   one per observer, allowed only for that observer's connection, holding what it sights.
	// - Data on the always-relevant PlayerState (its attribute sets): subobjects replicated with
	//   COND_NetGroup, which Iris sends to a connection only through a net condition group that
	//   allows it. One group per participant; teammates and current observers are members.
	NETWORK_TEST_CLASS(FogGate, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		FVeyraGreyboxLayout Layout;

		// Fixture values.
		static constexpr int32 PlayerCount = 3;
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr double NegativeCheckRealSeconds = 0.5;
		static constexpr double DamageAmount = 50.0;

		/** What the server knows about each client, by client index. */
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

		// Server-side gate state.
		TMap<EVeyraTeam, UE::Net::FNetObjectGroupHandle> SideGroups;
		TMap<int32, UE::Net::FNetObjectGroupHandle> SightingGroups;
		double HoldStartRealTime = 0.0;
		double MaxHealth = 0.0;

		BEFORE_ALL()
		{
			LoadOnDemandEngineModules();
		}

		BEFORE_EACH()
		{
			IgnoreLoginViewTargetRpc(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			MaxHealth = Tuning->Tuning.DeveloperLoadout.MaxHealth;
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(PlayerCount);
			BuildMatchNetwork(Network, PlayerCount);
		}

		AFTER_EACH()
		{
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		// Server helpers.

		static UReplicationSystem& ReplicationSystemOf(const FState& State)
		{
			return *UE::Net::FReplicationSystemUtil::GetReplicationSystem(State.World);
		}

		static UE::Net::FNetRefHandle HandleOf(const FState& State, const UObject* Object)
		{
			return ReplicationSystemOf(State).GetReplicationBridge()->GetReplicatedRefHandle(Object);
		}

		static uint32 ConnectionIdOf(const FState& State, int32 ClientIndex)
		{
			return ServerControllerOf(State, ClientIndex)->GetNetConnection()->GetConnectionHandle().GetParentConnectionId();
		}

		static AVeyraPlayerState& ServerParticipant(const FState& State, int32 ClientIndex)
		{
			return *ServerControllerOf(State, ClientIndex)->GetPlayerState<AVeyraPlayerState>();
		}

		static FName ParticipantGroup(int32 PlayerId)
		{
			return FName(*FString::Printf(TEXT("Veyra.Participant.%d"), PlayerId));
		}

		static UAttributeSet* VitalsOf(AVeyraPlayerState& Participant)
		{
			for (UAttributeSet* Set : Participant.GetAbilitySystemComponent()->GetSpawnedAttributes())
			{
				if (Set && Set->IsA<UVeyraVitalsSet>())
				{
					return Set;
				}
			}
			return nullptr;
		}

		// Client helpers.

		/** Whether this machine has the Vanguard of the participant with PlayerId. */
		static bool SeesVanguard(const UWorld* World, int32 PlayerId)
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

		/** Records each client's participant and picks the observer, bystander and enemy. */
		FPIENetworkComponent<FState>& IdentifyPlayers(FPIENetworkComponent<FState>& Chain)
		{
			return Chain.ThenServer(TEXT("Identify the players"), [this](FState& State) {
				Participants.Reset();
				for (int32 Index = 0; Index < PlayerCount; ++Index)
				{
					const AVeyraPlayerState& Participant = ServerParticipant(State, Index);
					Participants.Add({ Participant.GetPlayerId(), Participant.GetVeyraTeam() });
				}
				// Three players make one side of two and one of one.
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

		FPIENetworkComponent<FState>& HoldBriefly(FPIENetworkComponent<FState>& Chain, const TCHAR* Description)
		{
			return Chain
				.ThenServer([this](FState& State) { HoldStartRealTime = State.World->GetRealTimeSeconds(); })
				.UntilServer(Description, [this](FState& State) { return State.World->GetRealTimeSeconds() - HoldStartRealTime >= NegativeCheckRealSeconds; });
		}

		TEST_METHOD(EnemyVanguardsReachOnlyThePlayersWhoSeeThem)
		{
			FPIENetworkComponent<FState>& Chain = IdentifyPlayers(StartMatch(Network, Layout, EVeyraMatchPhase::Live))
				.ThenServer(TEXT("Gate every Vanguard"), [this](FState& State) {
					UReplicationSystem& System = ReplicationSystemOf(State);
					const UE::Net::FNetObjectFilterHandle FilterOut = System.GetFilterHandle(TEXT("NotRouted"));
					ASSERT_THAT(IsTrue(FilterOut != UE::Net::InvalidNetObjectFilterHandle));
					for (const EVeyraTeam Side : { EVeyraTeam::A, EVeyraTeam::B })
					{
						const UE::Net::FNetObjectGroupHandle Group = System.CreateGroup(Side == EVeyraTeam::A ? TEXT("Veyra.Side.A") : TEXT("Veyra.Side.B"));
						ASSERT_THAT(IsTrue(System.AddInclusionFilterGroup(Group)));
						SideGroups.Add(Side, Group);
					}
					for (int32 Index = 0; Index < PlayerCount; ++Index)
					{
						const uint32 Connection = ConnectionIdOf(State, Index);
						for (const TPair<EVeyraTeam, UE::Net::FNetObjectGroupHandle>& Side : SideGroups)
						{
							System.SetGroupFilterStatus(Side.Value, Connection,
								Side.Key == Participants[Index].Team ? UE::Net::ENetFilterStatus::Allow : UE::Net::ENetFilterStatus::Disallow);
						}
						const UE::Net::FNetObjectGroupHandle Sightings = System.CreateGroup(FName(*FString::Printf(TEXT("Veyra.Sightings.%u"), Connection)));
						ASSERT_THAT(IsTrue(System.AddInclusionFilterGroup(Sightings)));
						System.SetGroupFilterStatus(Sightings, Connection, UE::Net::ENetFilterStatus::Allow);
						SightingGroups.Add(Index, Sightings);

						const UE::Net::FNetRefHandle Vanguard = HandleOf(State, ServerParticipant(State, Index).GetPawn());
						ASSERT_THAT(IsTrue(System.SetFilter(Vanguard, FilterOut)));
						System.AddToGroup(SideGroups[Participants[Index].Team], Vanguard);
					}
				})
				.UntilClients(TEXT("Each client has its own side's Vanguards and no enemy's"), [this](FState& State) {
					const EVeyraTeam MySide = Participants[State.ClientIndex].Team;
					return Algo::AllOf(Participants, [&State, MySide](const FParticipant& Other) {
						return SeesVanguard(State.World, Other.PlayerId) == (Other.Team == MySide);
					});
				})
				.ThenServer(TEXT("The observer sights the enemy"), [this](FState& State) {
					ReplicationSystemOf(State).AddToGroup(SightingGroups[ObserverIndex], HandleOf(State, ServerParticipant(State, EnemyIndex).GetPawn()));
				})
				.UntilClients(TEXT("The observer receives the enemy"), [this](FState& State) {
					return State.ClientIndex != ObserverIndex || SeesVanguard(State.World, Participants[EnemyIndex].PlayerId);
				});
			HoldBriefly(Chain, TEXT("Give the sighting time to leak"))
				.ThenClients(TEXT("Only the observer has the enemy"), [this](FState& State) {
					ASSERT_THAT(IsTrue(SeesVanguard(State.World, Participants[EnemyIndex].PlayerId) == (State.ClientIndex != BystanderIndex)));
				})
				.ThenServer(TEXT("The observer loses sight"), [this](FState& State) {
					ReplicationSystemOf(State).RemoveFromGroup(SightingGroups[ObserverIndex], HandleOf(State, ServerParticipant(State, EnemyIndex).GetPawn()));
				})
				.UntilClients(TEXT("The enemy leaves the observer's client"), [this](FState& State) {
					return State.ClientIndex != ObserverIndex || !SeesVanguard(State.World, Participants[EnemyIndex].PlayerId);
				});
		}

		TEST_METHOD(EnemyVitalsReachOnlyThePlayersWhoSeeThem)
		{
			FPIENetworkComponent<FState>& Chain = IdentifyPlayers(StartMatch(Network, Layout, EVeyraMatchPhase::Live))
				.ThenServer(TEXT("Gate every participant's vitals"), [this](FState& State) {
					UE::Net::FNetConditionGroupManager& Groups = State.World->GetSubsystem<UNetworkSubsystem>()->GetNetConditionGroupManager();
					for (int32 Index = 0; Index < PlayerCount; ++Index)
					{
						AVeyraPlayerState& Participant = ServerParticipant(State, Index);
						UAbilitySystemComponent& Abilities = *Participant.GetAbilitySystemComponent();
						UAttributeSet* Vitals = VitalsOf(Participant);
						ASSERT_THAT(IsNotNull(Vitals));
						// GAS registered the set for everyone; register it again for its groups only.
						Abilities.RemoveReplicatedSubObject(Vitals);
						Abilities.AddReplicatedSubObject(Vitals, COND_NetGroup);
						Groups.RegisterSubObjectInGroup(Vitals, ParticipantGroup(Participant.GetPlayerId()));
						Groups.RegisterSubObjectInGroup(Vitals, UE::Net::NetGroupOwner);
						UE::Net::FReplicationSystemUtil::UpdateSubObjectGroupMemberships(Vitals, State.World);
					}
					for (int32 Index = 0; Index < PlayerCount; ++Index)
					{
						for (int32 Teammate = 0; Teammate < PlayerCount; ++Teammate)
						{
							if (Teammate != Index && Participants[Teammate].Team == Participants[Index].Team)
							{
								ServerControllerOf(State, Teammate)->IncludeInNetConditionGroup(ParticipantGroup(Participants[Index].PlayerId));
							}
						}
					}
				});
			HoldBriefly(Chain, TEXT("Let the new registration settle"))
				.ThenServer(TEXT("Damage the enemy"), [this](FState& State) {
					FVeyraRawDamageEvent Hit;
					Hit.Components.Add({ EVeyraDamageType::TrueDamage, DamageAmount });
					ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*ServerParticipant(State, ObserverIndex).GetAbilitySystemComponent(),
						*ServerParticipant(State, EnemyIndex).GetAbilitySystemComponent(), Hit)));
				})
				.UntilClients(TEXT("The enemy's own client sees its Health fall"), [this](FState& State) {
					return State.ClientIndex != EnemyIndex || SeenHealth(State.World, Participants[EnemyIndex].PlayerId) == MaxHealth - DamageAmount;
				});
			HoldBriefly(Chain, TEXT("Give the damage time to leak"))
				.ThenClients(TEXT("Nobody who cannot see the enemy learned its Health"), [this](FState& State) {
					if (State.ClientIndex != EnemyIndex)
					{
						ASSERT_THAT(IsTrue(SeenHealth(State.World, Participants[EnemyIndex].PlayerId) != MaxHealth - DamageAmount));
					}
				})
				.ThenServer(TEXT("The observer sights the enemy"), [this](FState& State) {
					ServerControllerOf(State, ObserverIndex)->IncludeInNetConditionGroup(ParticipantGroup(Participants[EnemyIndex].PlayerId));
				})
				.UntilClients(TEXT("The observer receives the enemy's current Health"), [this](FState& State) {
					return State.ClientIndex != ObserverIndex || SeenHealth(State.World, Participants[EnemyIndex].PlayerId) == MaxHealth - DamageAmount;
				});
			HoldBriefly(Chain, TEXT("Give the sighting time to leak"))
				.ThenClients(TEXT("Only the observer and the enemy know it"), [this](FState& State) {
					const bool bKnows = SeenHealth(State.World, Participants[EnemyIndex].PlayerId) == MaxHealth - DamageAmount;
					ASSERT_THAT(IsTrue(bKnows == (State.ClientIndex != BystanderIndex)));
				})
				.ThenServer(TEXT("The observer loses sight, and the enemy is hit again"), [this](FState& State) {
					ServerControllerOf(State, ObserverIndex)->RemoveFromNetConditionGroup(ParticipantGroup(Participants[EnemyIndex].PlayerId));
					FVeyraRawDamageEvent Hit;
					Hit.Components.Add({ EVeyraDamageType::TrueDamage, DamageAmount });
					ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*ServerParticipant(State, ObserverIndex).GetAbilitySystemComponent(),
						*ServerParticipant(State, EnemyIndex).GetAbilitySystemComponent(), Hit)));
				})
				.UntilClients(TEXT("The enemy's own client sees the second hit"), [this](FState& State) {
					return State.ClientIndex != EnemyIndex || SeenHealth(State.World, Participants[EnemyIndex].PlayerId) == MaxHealth - 2.0 * DamageAmount;
				});
			HoldBriefly(Chain, TEXT("Give the second hit time to leak"))
				.ThenClients(TEXT("The observer keeps the Health it last saw"), [this](FState& State) {
					if (State.ClientIndex == ObserverIndex)
					{
						ASSERT_THAT(IsTrue(SeenHealth(State.World, Participants[EnemyIndex].PlayerId) == MaxHealth - DamageAmount));
					}
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
