// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "AbilitySystemComponent.h"
#include "Rules/VeyraStructureRules.h"
#include "State/VeyraTeamFluxState.h"
#include "Tests/Net/VeyraBattlegroundNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "Tests/Net/VeyraVanguardNetTestHelpers.h"
#include "Tuning/VeyraFluxTuningSubsystem.h"
#include "VeyraCombatVerbs.h"

namespace VeyraNetTests
{
	// Veyra.Net.StructureReplication.*: the battleground's structures reach every client, and
	// destroying one grants its destroyers Team Flux through Match (ADR-011 §3, §4).
	NETWORK_TEST_CLASS(StructureReplication, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		FVeyraGreyboxLayout Greybox;

		// Fixture values: a short preparation, and damage enough to destroy anything.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr double Lethal = 1000000.0;

		BEFORE_EACH()
		{
			IgnoreLoginViewTargetRpc(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Greybox).IsEmpty()));
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

		TEST_METHOD(EveryClientSeesTheStructuresAndAFallenSpireGrantsFlux)
		{
			StartBattleground(Network, Greybox, EVeyraMatchPhase::Live)
				.UntilClients(TEXT("Every client sees every structure"), [](FState& State) {
					return SeenStructures(State.World).Num() == 14;
				})
				.ThenServer(TEXT("Team A destroys team B's outer Spire"), [](FState& State) {
					AVeyraPlayerState* Destroyer = nullptr;
					for (int32 Client = 0; Client < MatchClientCount; ++Client)
					{
						AVeyraPlayerState* Participant = ParticipantOf(State, Client);
						Destroyer = Participant && Participant->GetVeyraTeam() == EVeyraTeam::A ? Participant : Destroyer;
					}
					AVeyraStructure* Outer = State.World->GetSubsystem<UVeyraBattlegroundSubsystem>()->FindStructure(EVeyraTeam::B,
						EVeyraStructureKind::LaneSpire, EVeyraLane::Mid, 0);
					FVeyraRawDamageEvent Damage;
					Damage.Components.Add({ EVeyraDamageType::TrueDamage, Lethal });
					Damage.Delivery = EVeyraDamageDelivery::Developer;
					VeyraCombat::DealDamage(*Destroyer->GetAbilitySystemComponent(), *Outer->GetAbilitySystemComponent(), Damage);
				})
				.UntilClients(TEXT("Every client sees team A's permanent Flux, and the next Spire open"), [](FState& State) {
					const AVeyraTeamFluxState* Flux = AVeyraTeamFluxState::Find(State.World);
					const FVeyraTeamFluxView* TeamA = Flux ? Flux->Find(EVeyraTeam::A) : nullptr;
					bool bMiddleOpen = false;
					for (const AVeyraStructure* Structure : SeenStructures(State.World))
					{
						bMiddleOpen |= Structure->GetVeyraTeam() == EVeyraTeam::B && Structure->GetStructureKind() == EVeyraStructureKind::LaneSpire
							&& Structure->GetOrder() == 1 && !Structure->IsInvulnerable();
					}
					return TeamA && TeamA->Permanent == UVeyraFluxTuningSubsystem::Get().Grants.LaneSpire.Amount && bMiddleOpen;
				});
		}

		TEST_METHOD(EveryClientSeesBackdoorProtectionRise)
		{
			StartBattleground(Network, Greybox, EVeyraMatchPhase::Live)
				.UntilClients(TEXT("With no Fluxborn near, every client sees each protected structure's protection rise"), [](FState& State) {
					const TArray<AVeyraStructure*> Structures = SeenStructures(State.World);
					return Structures.Num() > 0 && !Structures.ContainsByPredicate([](const AVeyraStructure* Structure) {
						return VeyraStructureRules::HasBackdoorProtection(Structure->GetStructureKind()) && !(Structure->GetBackdoorProtection() > 0.0);
					});
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
