// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "EngineUtils.h"
#include "Inventory/VeyraInventoryComponent.h"
#include "Shop/VeyraShopSubsystem.h"
#include "Tests/Net/VeyraBattlegroundNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "Tests/World/VeyraBattlegroundTestLayout.h"
#include "Tuning/VeyraFluxTuningSubsystem.h"
#include "Tuning/VeyraItemsTuningSubsystem.h"
#include "VeyraPlayerState.h"
#include "VeyraTeamFluxSubsystem.h"
#include "Wells/VeyraFluxWell.h"
#include "Wells/VeyraFluxWellSubsystem.h"

namespace VeyraNetTests
{
	// Veyra.Net.FluxWellSecure.*: a Vanguard standing at an open Flux Well drains it by presence and
	// secures it; Match grants its side the Well's Team Flux and refills its side's Flux Flasks, and
	// every client sees the Well's cycle (Battleground Bible §6; ADR-014 §4, §6; ADR-023 §6).
	NETWORK_TEST_CLASS(FluxWellSecure, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		TUniquePtr<VeyraWorldTests::FScopedWorldTuning> WorldTuning;
		FVeyraGreyboxLayout Greybox;

		// Fixture values: a short preparation, and one Well on the compact river, open at once and drained
		// fast by one Vanguard.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr double SiteX = 600.0;
		static constexpr double SiteY = -600.0;
		static constexpr double Radius = 300.0;
		static constexpr double FastDrain = 4000.0;
		static constexpr double TickSeconds = 0.25;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Greybox).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			WorldTuning = MakeUnique<VeyraWorldTests::FScopedWorldTuning>();
			FVeyraFluxWellsTuning& Wells = WorldTuning->Tuning.FluxWells;
			Wells.Sites = { { SiteX, SiteY } };
			Wells.Timing.OpenSeconds = 0.0;
			Wells.Radius = Radius;
			Wells.Presence.TickSeconds = TickSeconds;
			Wells.Presence.DrainPerSecond = FastDrain;
			// No camps: this test is the Wells'.
			WorldTuning->Tuning.Wildlife.Camps.Reset();
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			WorldTuning.Reset();
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		static const AVeyraFluxWell* SeenWell(const UWorld* World)
		{
			for (TActorIterator<AVeyraFluxWell> It(World); It; ++It)
			{
				return *It;
			}
			return nullptr;
		}

		static FVeyraContentId Flask()
		{
			return FVeyraContentId::FromText(TEXT("flux_flask")).GetValue();
		}

		static int32 FlaskCharges(const AVeyraPlayerState& Participant)
		{
			const FVeyraInventorySlot* Held = Participant.FindComponentByClass<UVeyraInventoryComponent>()->GetSlots().FindByPredicate(
				[](const FVeyraInventorySlot& Slot) { return Slot.Item == Flask(); });
			return Held ? Held->Charges : INDEX_NONE;
		}

		TEST_METHOD(PresenceSecuresTheWellAndItsSideTakesTheFlux)
		{
			StartBattleground(Network, Greybox, EVeyraMatchPhase::Live)
				.UntilClients(TEXT("Every client sees the Well open"), [](FState& State) {
					const AVeyraFluxWell* Well = SeenWell(State.World);
					return Well && Well->GetState() == EVeyraFluxWellState::Open;
				})
				.ThenServer(TEXT("Each Vanguard buys a Flux Flask at its fountain and drinks from it"), [this](FState& State) {
					UVeyraShopSubsystem* Shop = State.World->GetSubsystem<UVeyraShopSubsystem>();
					for (int32 Client = 0; Client < MatchClientCount; ++Client)
					{
						AVeyraPlayerState& Participant = *ServerControllerOf(State, Client)->GetPlayerState<AVeyraPlayerState>();
						Shop->SetAtFountain(Participant, true);
						ASSERT_THAT(IsTrue(Shop->Buy(Participant, Flask()) == EVeyraShopRefusal::None, TEXT("the committed catalog's Flux Flask")));
						const int32 Slot = Participant.FindComponentByClass<UVeyraInventoryComponent>()->GetSlots().IndexOfByPredicate(
							[](const FVeyraInventorySlot& Held) { return Held.Item == Flask(); });
						ASSERT_THAT(IsTrue(Shop->UseConsumable(Participant, Slot) == EVeyraShopRefusal::None));
					}
				})
				.ThenServer(TEXT("Team A's Vanguard stands at the Well"), [](FState& State) {
					APawn* Body = ServerControllerOf(State, 0)->GetPlayerState<AVeyraPlayerState>()->GetPawn();
					const FVector Spot(SiteX + Radius / 2.0, SiteY, Body->GetActorLocation().Z);
					Body->TeleportTo(Spot, Body->GetActorRotation());
				})
				.UntilServer(TEXT("It secures the Well, and its side takes the Well's Team Flux"), [](FState& State) {
					const AVeyraFluxWell* Well = State.World->GetSubsystem<UVeyraFluxWellSubsystem>()->GetWells()[0];
					const EVeyraTeam Side = ServerControllerOf(State, 0)->GetPlayerState<AVeyraPlayerState>()->GetVeyraTeam();
					const double Granted = UVeyraFluxTuningSubsystem::Get().Grants.FluxWell.Amount;
					return Well->GetState() == EVeyraFluxWellState::Respawning && State.World->GetSubsystem<UVeyraTeamFluxSubsystem>()->GetActive(Side) >= Granted;
				})
				.ThenServer(TEXT("Its side's Flux Flask is full again; the other side's is not"), [this](FState& State) {
					const int32 Full = UVeyraItemsTuningSubsystem::Get().Consumables.FindChecked(Flask()).Charges;
					const AVeyraPlayerState& Securer = *ServerControllerOf(State, 0)->GetPlayerState<AVeyraPlayerState>();
					const AVeyraPlayerState& Other = *ServerControllerOf(State, 1)->GetPlayerState<AVeyraPlayerState>();
					ASSERT_THAT(IsTrue(Securer.GetVeyraTeam() != Other.GetVeyraTeam()));
					ASSERT_THAT(AreEqual(Full, FlaskCharges(Securer)));
					ASSERT_THAT(AreEqual(Full - 1, FlaskCharges(Other), TEXT("still at its fountain, it has not arrived again")));
				})
				.UntilClients(TEXT("Every client sees it wait out its cycle"), [](FState& State) {
					const AVeyraFluxWell* Well = SeenWell(State.World);
					return Well && Well->GetState() == EVeyraFluxWellState::Respawning && Well->GetOpensAt() > 0.0;
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
