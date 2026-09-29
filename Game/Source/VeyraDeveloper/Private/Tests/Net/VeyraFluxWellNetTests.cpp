// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "EngineUtils.h"
#include "Tests/Net/VeyraBattlegroundNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "Tests/World/VeyraBattlegroundTestLayout.h"
#include "Tuning/VeyraFluxTuningSubsystem.h"
#include "VeyraPlayerState.h"
#include "VeyraTeamFluxSubsystem.h"
#include "Wells/VeyraFluxWell.h"
#include "Wells/VeyraFluxWellSubsystem.h"

namespace VeyraNetTests
{
	// Veyra.Net.FluxWellSecure.*: a Vanguard standing at an open Flux Well drains it by presence and
	// secures it; Match grants its side the Well's Team Flux, and every client sees the Well's cycle
	// (Battleground Bible §6; ADR-014 §4, §6).
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

		TEST_METHOD(PresenceSecuresTheWellAndItsSideTakesTheFlux)
		{
			StartBattleground(Network, Greybox, EVeyraMatchPhase::Live)
				.UntilClients(TEXT("Every client sees the Well open"), [](FState& State) {
					const AVeyraFluxWell* Well = SeenWell(State.World);
					return Well && Well->GetState() == EVeyraFluxWellState::Open;
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
				.UntilClients(TEXT("Every client sees it wait out its cycle"), [](FState& State) {
					const AVeyraFluxWell* Well = SeenWell(State.World);
					return Well && Well->GetState() == EVeyraFluxWellState::Respawning && Well->GetOpensAt() > 0.0;
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
